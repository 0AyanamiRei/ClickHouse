#pragma once
#include <Processors/Merges/Algorithms/IMergingAlgorithmWithSharedChunks.h>
#include <Processors/Merges/Algorithms/MergedData.h>
#include <Processors/Transforms/ColumnGathererTransform.h>
#include <Processors/Merges/Algorithms/RowRef.h>

namespace Poco
{
class Logger;
}

namespace DB
{

struct ChunkSelectFinalIndices : public ChunkInfo
{
    ColumnPtr column_holder;
    const ColumnUInt64 * select_final_indices;
    explicit ChunkSelectFinalIndices(MutableColumnPtr select_final_indices_) : column_holder(std::move(select_final_indices_))
    {
        select_final_indices = typeid_cast<const ColumnUInt64 *>(column_holder.get());
    }
};

/** Merges several sorted inputs into one.
  * For each group of consecutive identical values of the primary key (the columns by which the data is sorted),
  *  keeps row with max `version` value.
  */
class ReplacingSortedAlgorithm final : public IMergingAlgorithmWithSharedChunks
{
public:
    ReplacingSortedAlgorithm(
        const Block & header, size_t num_inputs,
        SortDescription description_,
        const String & is_deleted_column,
        const String & version_column,
        size_t max_block_size_rows,
        size_t max_block_size_bytes,
        WriteBuffer * out_row_sources_buf_ = nullptr,
        bool use_average_block_sizes = false,
        bool cleanup = false,
        size_t * cleanedup_rows_count = nullptr,
        bool use_skipping_final = false);

    const char * getName() const override { return "ReplacingSortedAlgorithm"; }
    Status merge() override;

private:
    MergedData merged_data;

    ssize_t is_deleted_column_number = -1;
    ssize_t version_column_number = -1;
    bool cleanup = false;
    size_t * cleanedup_rows_count = nullptr;

    bool use_skipping_final; /// Either we use skipping final algorithm
    std::queue<detail::SharedChunkPtr> to_be_emitted;   /// To save chunks when using skipping final

    using RowRef = detail::RowRefWithOwnedChunk;
    static constexpr size_t max_row_refs = 2; /// last, current.
    RowRef selected_row; /// Last row with maximum version for current primary key.
    size_t max_pos = 0; /// The position (into current_row_sources) of the row with the highest version.

    /// Sources of rows with the current primary key.
    PODArray<RowSourcePart> current_row_sources;

    void insertRow();

    void saveChunkForSkippingFinalFromSource(size_t current_source_index);
    void saveChunkForSkippingFinalFromSelectedRow();
    static Status emitChunk(detail::SharedChunkPtr & chunk, bool finished = false);
};

}
