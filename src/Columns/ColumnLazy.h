#pragma once

#include <cstring>
#include <cassert>

#include <Columns/IColumn.h>
#include <Columns/IColumnImpl.h>
#include <Common/PODArray.h>
#include <Common/assert_cast.h>
#include <Core/ColumnsWithTypeAndName.h>
#include <Core/Field.h>


namespace DB
{

class IColumnLazyHelper;
using ColumnLazyHelperPtr = std::shared_ptr<IColumnLazyHelper>;

/** Column for lazy materialization.
  */
class ColumnLazy final : public COWHelper<IColumn, ColumnLazy>
{
private:
    friend class COWHelper<IColumn, ColumnLazy>;

    using CapturedColumns = std::vector<WrappedPtr>;
    CapturedColumns captured_columns;
    ColumnLazyHelperPtr column_lazy_helper;
    size_t s = 0;

    explicit ColumnLazy(MutableColumns && mutable_columns_);
    ColumnLazy(const ColumnLazy &) = default;

public:
    using Base = COWHelper<IColumn, ColumnLazy>;

    static Ptr create(const Columns & columns, ColumnLazyHelperPtr column_lazy_helper);
    static Ptr create(const CapturedColumns & columns, ColumnLazyHelperPtr column_lazy_helper);
    static Ptr create(size_t s = 0);
    static Ptr create(Columns && arg, ColumnLazyHelperPtr column_lazy_helper)
    {
        return create(arg, column_lazy_helper);
    }

    template <typename Arg>
    requires std::is_rvalue_reference_v<Arg &&>
    static MutablePtr create(Arg && arg) { return Base::create(std::forward<Arg>(arg)); }

    const char * getFamilyName() const override { return "Lazy"; }
    TypeIndex getDataType() const override { return TypeIndex::Nothing; }

    MutableColumnPtr cloneResized(size_t size) const override;

    size_t size() const override
    {
        if (column_lazy_helper)
            return captured_columns[0]->size();
        return s;
    }

    Field operator[](size_t n) const override;
    void get(size_t n, Field & res) const override;

    bool isDefaultAt(size_t n) const override;
    StringRef getDataAt(size_t n) const override;
    void insertData(const char * pos, size_t length) override;
    void insert(const Field & x) override;
    bool tryInsert(const Field & x) override;
    void insertFrom(const IColumn & src_, size_t n) override;
    void insertDefault() override;
    void popBack(size_t n) override;
    const char * deserializeAndInsertFromArena(const char * pos) override;
    const char * skipSerializedInArena(const char * pos) const override;
    void updateHashWithValue(size_t n, SipHash & hash) const override;
    void updateWeakHash32(WeakHash32 & hash) const override;
    void updateHashFast(SipHash & hash) const override;
    void insertRangeFrom(const IColumn & src, size_t start, size_t length) override;
    ColumnPtr filter(const Filter & filt, ssize_t result_size_hint) const override;
    void expand(const Filter & mask, bool inverted) override;
    ColumnPtr permute(const Permutation & perm, size_t limit) const override;
    ColumnPtr index(const IColumn & indexes, size_t limit) const override;
    ColumnPtr replicate(const Offsets & offsets) const override;
    MutableColumns scatter(ColumnIndex num_columns, const Selector & selector) const override;
    void gather(ColumnGathererStream & gatherer_stream) override;
    int compareAt(size_t n, size_t m, const IColumn & rhs, int nan_direction_hint) const override;
    void compareColumn(const IColumn & rhs, size_t rhs_row_num,
                       PaddedPODArray<UInt64> * row_indexes, PaddedPODArray<Int8> & compare_results,
                       int direction, int nan_direction_hint) const override;
    int compareAtWithCollation(size_t n, size_t m, const IColumn & rhs, int nan_direction_hint, const Collator & collator) const override;
    bool hasEqualValues() const override;
    void getExtremes(Field & min, Field & max) const override;
    void getPermutation(IColumn::PermutationSortDirection direction, IColumn::PermutationSortStability stability,
                    size_t limit, int nan_direction_hint, IColumn::Permutation & res) const override;
    void updatePermutation(IColumn::PermutationSortDirection direction, IColumn::PermutationSortStability stability,
                    size_t limit, int nan_direction_hint, IColumn::Permutation & res, EqualRanges & equal_ranges) const override;
    void getPermutationWithCollation(const Collator & collator, IColumn::PermutationSortDirection direction, IColumn::PermutationSortStability stability,
                    size_t limit, int nan_direction_hint, IColumn::Permutation & res) const override;
    void updatePermutationWithCollation(const Collator & collator, IColumn::PermutationSortDirection direction, IColumn::PermutationSortStability stability,
                    size_t limit, int nan_direction_hint, IColumn::Permutation & res, EqualRanges& equal_ranges) const override;
    void reserve(size_t n) override;
    void ensureOwnership() override;
    size_t byteSize() const override;
    size_t byteSizeAt(size_t n) const override;
    size_t allocatedBytes() const override;
    void protect() override;
    void forEachSubcolumnRecursively(RecursiveMutableColumnCallback callback) override;
    bool structureEquals(const IColumn & rhs) const override;
    bool isCollationSupported() const override;
    ColumnPtr compress() const override;
    double getRatioOfDefaultRows(double sample_ratio) const override;
    UInt64 getNumberOfDefaultRows() const override;
    void getIndicesOfNonDefaultRows(Offsets & indices, size_t from, size_t limit) const override;
    void finalize() override;
    bool isFinalized() const override;

    const CapturedColumns & getColumns() const { return captured_columns; }
    ColumnLazyHelperPtr getColumnLazyHelper() const { return column_lazy_helper; }

    void transform(ColumnsWithTypeAndName & res_columns) const;
};

}
