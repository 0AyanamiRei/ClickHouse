#include <Storages/MergeTree/Compaction/MergePredicates/ReplicatedMergeTreeMergePredicate.h>
#include <Storages/MergeTree/Compaction/PartsCollectors/ReplicatedMergeTreePartsCollector.h>
#include <Storages/MergeTree/Compaction/PartsCollectors/Common.h>

namespace DB
{

namespace
{

MergeTreeDataPartsVector collectInitial(const MergeTreeData & data)
{
    return data.getDataPartsVectorForInternalUsage();
}

auto constructPreconditionsPredicate(const ReplicatedMergeTreeMergePredicatePtr & merge_pred)
{
    auto predicate = [merge_pred](const MergeTreeDataPartPtr & part) -> std::expected<void, PreformattedMessage>
    {
        chassert(merge_pred);
        return merge_pred->canUsePartInMerges(part);
    };

    return predicate;
}

std::vector<MergeTreeDataPartsVector> splitPartsByPreconditions(MergeTreeDataPartsVector && parts, const ReplicatedMergeTreeMergePredicatePtr & merge_pred)
{
    return splitRangeByPredicate(std::move(parts), constructPreconditionsPredicate(merge_pred));
}

std::expected<void, PreformattedMessage> checkAllParts(const MergeTreeDataPartsVector & parts, const ReplicatedMergeTreeMergePredicatePtr & merge_pred)
{
    return checkAllPartsSatisfyPredicate(parts, constructPreconditionsPredicate(merge_pred));
}

}

ReplicatedMergeTreePartsCollector::ReplicatedMergeTreePartsCollector(const StorageReplicatedMergeTree & storage_, ReplicatedMergeTreeMergePredicatePtr merge_pred_)
    : storage(storage_)
    , merge_pred(std::move(merge_pred_))
{
}

PartsRanges ReplicatedMergeTreePartsCollector::grabAllPossibleRanges(
    const StorageMetadataPtr & metadata_snapshot,
    const StoragePolicyPtr & storage_policy,
    const time_t & current_time,
    const std::optional<PartitionIdsHint> & partitions_hint) const
{
    auto parts = filterByPartitions(collectInitial(storage), partitions_hint);
    auto ranges = splitPartsByPreconditions(std::move(parts), merge_pred);
    return constructPartsRanges(std::move(ranges), metadata_snapshot, storage_policy, current_time);
}

std::expected<PartsRange, PreformattedMessage> ReplicatedMergeTreePartsCollector::grabAllPartsInsidePartition(
    const StorageMetadataPtr & metadata_snapshot,
    const StoragePolicyPtr & storage_policy,
    const time_t & current_time,
    const std::string & partition_id) const
{
    auto parts = filterByPartitions(collectInitial(storage), PartitionIdsHint{partition_id});
    if (auto result = checkAllParts(parts, merge_pred); !result)
        return std::unexpected(std::move(result.error()));

    auto ranges = constructPartsRanges({std::move(parts)}, metadata_snapshot, storage_policy, current_time);
    chassert(ranges.size() == 1);

    return std::move(ranges.front());
}

}
