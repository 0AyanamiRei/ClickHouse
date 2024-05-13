#pragma once

#include <Interpreters/ProfileEventsExt.h>
#include <base/types.h>
#include "Common/ProfileEvents.h"
#include <Common/Stopwatch.h>

#include <map>
#include <mutex>
#include <string_view>
#include <unordered_map>

namespace DB
{

class WriteBufferFromFileDescriptor;
class Block;

class ProgressTable
{
public:
    /// Write progress table with metrics.
    void writeTable(WriteBufferFromFileDescriptor & message);
    void clearTableOutput(WriteBufferFromFileDescriptor & message);
    void writeFinalTable();

    /// Update the metric values. They can be updated from:
    /// onProfileEvents in clickhouse-client;
    void updateTable(const Block & block);

    /// Reset progress table values.
    void resetTable();

private:
    class MetricInfo
    {
    public:
        explicit MetricInfo(ProfileEvents::Type t);

        void updateValue(Int64 new_value, double new_time);
        double calculateProgress(double time_now) const;
        double getValue() const;

    private:
        const ProfileEvents::Type type;

        struct Snapshot
        {
            Int64 value = 0;
            double time = 0;
        };

        /// The previous and current snapshots are used to calculateProgress.
        /// They contain outdated by about a second information.
        /// The new snapshot is used to updateValue and getValue.
        /// If you use a new snapshot to calculate progress, then the time difference between
        /// the previous update will be very small, so progress will jitter.
        Snapshot prev_shapshot;
        Snapshot cur_shapshot;
        Snapshot new_snapshot;
    };

    class MetricInfoPerHost
    {
    public:
        using HostName = String;

        void updateHostValue(const HostName & host, ProfileEvents::Type type, Int64 new_value, double new_time);
        double getSummaryValue();
        double getSummaryProgress(double time_now);
        double getMaxProgress() const;

    private:
        std::unordered_map<HostName, MetricInfo> host_to_metric;
        double max_progress = 0;
    };

    size_t tableSize() const;

    using MetricName = String;

    /// The server periodically sends Block with profile events.
    /// This information is stored here.
    std::map<MetricName, MetricInfoPerHost> metrics;

    /// It is possible concurrent access to the metrics.
    std::mutex mutex;

    /// Track query execution time on client.
    Stopwatch watch;

    bool written_first_block = false;

    size_t column_event_name_width = 20;

    /// The value type for each profile event for a readable output.
    static const std::unordered_map<std::string_view, ProfileEvents::ValueType> event_to_value_type;

    static constexpr std::string_view COLUMN_EVENT_NAME = "Event name";
    static constexpr std::string_view COLUMN_VALUE = "Value";
    static constexpr std::string_view COLUMN_PROGRESS = "Progress";
    static constexpr size_t COLUMN_VALUE_WIDTH = 20;
    static constexpr size_t COLUMN_PROGRESS_WIDTH = 20;
};

}
