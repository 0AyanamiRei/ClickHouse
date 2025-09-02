#pragma once
#include <vector>
#include <Storages/MergeTree/MergeTreeIndices.h>
#include <Columns/IColumn.h>
#include <Formats/MarkInCompressedFile.h>
#include <Common/HashTable/HashMap.h>
#include <Interpreters/GinQueryString.h>
#include <Interpreters/BloomFilter.h>
#include <Interpreters/ITokenExtractor.h>
#include <absl/container/flat_hash_set.h>

namespace DB
{

class MergeTreeIndexReader;
class IMergeTreeIndexCondition;

struct MergeTreeIndexTextParams
{
    size_t dictionary_block_size = 0;
    size_t bloom_filter_bits_per_row = 0;
    size_t bloom_filter_num_hashes = 0;
};

using PostingList = roaring::Roaring;

struct CompressedPostings
{
public:
    static constexpr size_t max_raw_deltas = 4;
    static constexpr size_t num_cells_for_raw_deltas = max_raw_deltas * sizeof(UInt32) / sizeof(UInt64);
    using RawDeltas = std::array<UInt32, max_raw_deltas>;

    CompressedPostings(UInt8 delta_bits_, UInt32 cardinality_);
    explicit CompressedPostings(const roaring::Roaring & postings);

    UInt32 getDelta(size_t idx) const;
    UInt32 getCardinality() const { return cardinality; }
    bool empty() const { return cardinality == 0; }
    UInt8 getDeltaBits() const { return delta_bits; }
    bool hasRawDeltas() const { return cardinality <= max_raw_deltas; }

    void serialize(WriteBuffer & ostr) const;
    void deserialize(ReadBuffer & istr);

    struct Iterator
    {
    public:
        explicit Iterator(const CompressedPostings & postings_)
            : postings(&postings_), row(postings->empty() ? 0 : postings->getDelta(0))
        {
        }

        Iterator() : postings(nullptr), idx(0), row(0) {}

        UInt32 getRow() const { return row; }
        bool isValid() const { return idx < postings->getCardinality(); }

        void next()
        {
            ++idx;
            row += postings->getDelta(idx);
        }

    private:
        const CompressedPostings * postings;
        UInt32 idx = 0;
        UInt32 row = 0;
    };

    Iterator getIterator() const { return Iterator(*this); }

private:
    UInt8 delta_bits;
    UInt32 cardinality;
    std::vector<UInt64> deltas_buffer;
};

struct TokenInfo
{
public:
    struct FuturePostings
    {
        UInt32 cardinality = 0;
        UInt8 delta_bits = 0;
        UInt64 offset_in_file;

        UInt32 getCardinality() const { return cardinality; }
    };

    TokenInfo() : postings(FuturePostings{}) {}
    explicit TokenInfo(CompressedPostings postings_) : postings(std::move(postings_)) {}
    explicit TokenInfo(FuturePostings postings_) : postings(std::move(postings_)) {}

    UInt32 getCardinality() const;
    bool empty() const { return getCardinality() == 0; }

    bool hasEmbeddedPostings() const { return std::holds_alternative<CompressedPostings>(postings); }
    bool hasFuturePostings() const { return std::holds_alternative<FuturePostings>(postings); }

    CompressedPostings & getEmbeddedPostings() { return std::get<CompressedPostings>(postings); }
    FuturePostings & getFuturePostings() { return std::get<FuturePostings>(postings); }

    const CompressedPostings & getEmbeddedPostings() const { return std::get<CompressedPostings>(postings); }
    const FuturePostings & getFuturePostings() const { return std::get<FuturePostings>(postings); }

private:
    std::variant<CompressedPostings, FuturePostings> postings;
};

struct DictionaryBlockBase
{
    ColumnPtr tokens;

    DictionaryBlockBase() = default;
    explicit DictionaryBlockBase(ColumnPtr tokens_) : tokens(std::move(tokens_)) {}

    bool empty() const;
    size_t size() const;

    size_t lowerBound(const StringRef & token) const;
    size_t upperBound(const StringRef & token) const;
    std::optional<size_t> binarySearch(const StringRef & token) const;
};

struct DictionarySparseIndex : public DictionaryBlockBase
{
    DictionarySparseIndex() = default;
    DictionarySparseIndex(ColumnPtr tokens_, ColumnPtr offsets_in_file_);
    UInt64 getOffsetInFile(size_t idx) const;

    ColumnPtr offsets_in_file;
};

struct DictionaryBlock : public DictionaryBlockBase
{
    DictionaryBlock() = default;
    DictionaryBlock(ColumnPtr tokens_, std::vector<TokenInfo> token_infos_);

    std::vector<TokenInfo> token_infos;
};

struct MergeTreeIndexGranuleText final : public IMergeTreeIndexGranule
{
public:
    using TokensMap = absl::flat_hash_map<StringRef, TokenInfo>;

    explicit MergeTreeIndexGranuleText(MergeTreeIndexTextParams params_);
    ~MergeTreeIndexGranuleText() override = default;

    void serializeBinary(WriteBuffer & ostr) const override;
    void deserializeBinary(ReadBuffer & istr, MergeTreeIndexVersion version) override;
    void deserializeBinaryWithMultipleStreams(IndexInputStreams & streams, IndexDeserializationState & state) override;

    bool empty() const override { return total_rows == 0; }
    size_t memoryUsageBytes() const override { return 0; }
    bool hasAllTokensFromQuery(const GinQueryString & query) const;
    const TokensMap & getRemainingTokens() const { return remaining_tokens; }
    void resetAfterAnalysis();

private:
    void deserializeBloomFilter(ReadBuffer & istr);
    void deserializeSparseIndex(ReadBuffer & istr);

    void analyzeBloomFilter(const IMergeTreeIndexCondition & condition);
    void analyzeDictionary(IndexReaderStream & stream, IndexDeserializationState & state);

    /// Returns true if granule can be skipped completely due to missed token.
    bool processMissedToken(const StringRef & token, TextSearchMode global_search_mode);

    MergeTreeIndexTextParams params;
    size_t total_rows = 0;
    BloomFilter bloom_filter;
    DictionarySparseIndex sparse_index;
    TokensMap remaining_tokens;
};

struct MergeTreeIndexGranuleTextWritable : public IMergeTreeIndexGranule
{
    MergeTreeIndexGranuleTextWritable(
        size_t dictionary_block_size_,
        size_t total_rows_,
        BloomFilter bloom_filter_,
        std::vector<StringRef> tokens_,
        std::vector<PostingList> posting_lists_,
        std::unique_ptr<Arena> arena_);

    ~MergeTreeIndexGranuleTextWritable() override = default;

    void serializeBinary(WriteBuffer & ostr) const override;
    void serializeBinaryWithMultipleStreams(IndexOutputStreams & streams) const override;
    void deserializeBinary(ReadBuffer & istr, MergeTreeIndexVersion version) override;

    bool empty() const override { return tokens.empty(); }
    size_t memoryUsageBytes() const override { return 0; }

    size_t dictionary_block_size;
    size_t total_rows;
    BloomFilter bloom_filter;
    std::vector<StringRef> tokens;
    std::vector<PostingList> posting_lists;
    std::unique_ptr<Arena> arena;
};

struct MergeTreeIndexTextGranuleBuilder
{
    explicit MergeTreeIndexTextGranuleBuilder(MergeTreeIndexTextParams params_, TokenExtractorPtr token_extractor_);

    void addDocument(StringRef document);
    std::unique_ptr<MergeTreeIndexGranuleTextWritable> build();
    bool empty() const { return current_row == 0; }

    MergeTreeIndexTextParams params;
    TokenExtractorPtr token_extractor;

    using PostingListRawPtr = PostingList *;
    using TokensMap = HashMap<StringRef, PostingListRawPtr>;

    UInt64 current_row = 0;
    TokensMap tokens_map;
    std::list<PostingList> posting_lists;
    std::unique_ptr<Arena> arena;
};

struct MergeTreeIndexAggregatorText final : IMergeTreeIndexAggregator
{
    MergeTreeIndexAggregatorText(String index_column_name_, MergeTreeIndexTextParams params_, TokenExtractorPtr token_extractor_);
    ~MergeTreeIndexAggregatorText() override = default;

    bool empty() const override { return !granule_builder || granule_builder->empty(); }
    MergeTreeIndexGranulePtr getGranuleAndReset() override;
    void update(const Block & block, size_t * pos, size_t limit) override;

    String index_column_name;
    MergeTreeIndexTextParams params;
    TokenExtractorPtr token_extractor;
    std::optional<MergeTreeIndexTextGranuleBuilder> granule_builder;
};

class MergeTreeIndexText final : public IMergeTreeIndex
{
public:
    MergeTreeIndexText(
        const IndexDescription & index_,
        MergeTreeIndexTextParams params_,
        std::unique_ptr<ITokenExtractor> token_extractor_);

    ~MergeTreeIndexText() override = default;

    IndexSubstreams getSubstreams() const override;
    MergeTreeIndexFormat getDeserializedFormat(const IDataPartStorage & data_part_storage, const std::string & path_prefix) const override;
    bool hasHeavyGranules() const override { return true; }

    MergeTreeIndexGranulePtr createIndexGranule() const override;
    MergeTreeIndexAggregatorPtr createIndexAggregator(const MergeTreeWriterSettings & settings) const override;
    MergeTreeIndexConditionPtr createIndexCondition(const ActionsDAG::Node * predicate, ContextPtr context) const override;

    MergeTreeIndexTextParams params;
    std::unique_ptr<ITokenExtractor> token_extractor;
};

}
