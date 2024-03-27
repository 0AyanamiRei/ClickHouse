#include <Disks/ObjectStorages/HDFS/HDFSObjectStorage.h>

#include <IO/copyData.h>
#include <Storages/ObjectStorage/HDFS/WriteBufferFromHDFS.h>
#include <Storages/ObjectStorage/HDFS/HDFSCommon.h>

#include <Storages/ObjectStorage/HDFS/ReadBufferFromHDFS.h>
#include <Disks/IO/ReadBufferFromRemoteFSGather.h>
#include <Common/getRandomASCIIString.h>
#include <Common/logger_useful.h>


#if USE_HDFS

namespace DB
{

namespace ErrorCodes
{
    extern const int UNSUPPORTED_METHOD;
    extern const int HDFS_ERROR;
    extern const int ACCESS_DENIED;
    extern const int LOGICAL_ERROR;
}

void HDFSObjectStorage::shutdown()
{
}

void HDFSObjectStorage::startup()
{
}

ObjectStorageKey HDFSObjectStorage::generateObjectKeyForPath(const std::string & /* path */) const
{
    /// what ever data_source_description.description value is, consider that key as relative key
    return ObjectStorageKey::createAsRelative(hdfs_root_path, getRandomASCIIString(32));
}

bool HDFSObjectStorage::exists(const StoredObject & object) const
{
    // const auto & path = object.remote_path;
    // const size_t begin_of_path = path.find('/', path.find("//") + 2);
    // const String remote_fs_object_path = path.substr(begin_of_path);
    return (0 == hdfsExists(hdfs_fs.get(), object.remote_path.c_str()));
}

std::unique_ptr<ReadBufferFromFileBase> HDFSObjectStorage::readObject( /// NOLINT
    const StoredObject & object,
    const ReadSettings & read_settings,
    std::optional<size_t>,
    std::optional<size_t>) const
{
    return std::make_unique<ReadBufferFromHDFS>(hdfs_root_path, object.remote_path, config, patchSettings(read_settings));
}

std::unique_ptr<ReadBufferFromFileBase> HDFSObjectStorage::readObjects( /// NOLINT
    const StoredObjects & objects,
    const ReadSettings & read_settings,
    std::optional<size_t>,
    std::optional<size_t>) const
{
    auto disk_read_settings = patchSettings(read_settings);
    auto read_buffer_creator =
        [this, disk_read_settings]
        (bool /* restricted_seek */, const std::string & path) -> std::unique_ptr<ReadBufferFromFileBase>
    {
        // size_t begin_of_path = path.find('/', path.find("//") + 2);
        // auto hdfs_path = path.substr(begin_of_path);
        // auto hdfs_uri = path.substr(0, begin_of_path);

        return std::make_unique<ReadBufferFromHDFS>(
            hdfs_root_path, path, config, disk_read_settings, /* read_until_position */0, /* use_external_buffer */true);
    };

    return std::make_unique<ReadBufferFromRemoteFSGather>(
        std::move(read_buffer_creator), objects, "hdfs:", disk_read_settings, nullptr, /* use_external_buffer */false);
}

std::unique_ptr<WriteBufferFromFileBase> HDFSObjectStorage::writeObject( /// NOLINT
    const StoredObject & object,
    WriteMode mode,
    std::optional<ObjectAttributes> attributes,
    size_t buf_size,
    const WriteSettings & write_settings)
{
    if (attributes.has_value())
        throw Exception(
            ErrorCodes::UNSUPPORTED_METHOD,
            "HDFS API doesn't support custom attributes/metadata for stored objects");

    auto path = object.remote_path.starts_with('/') ? object.remote_path.substr(1) : object.remote_path;
    path = fs::path(hdfs_root_path) / path;

    /// Single O_WRONLY in libhdfs adds O_TRUNC
    return std::make_unique<WriteBufferFromHDFS>(
        path, config, settings->replication, patchSettings(write_settings), buf_size,
        mode == WriteMode::Rewrite ? O_WRONLY : O_WRONLY | O_APPEND);
}


/// Remove file. Throws exception if file doesn't exists or it's a directory.
void HDFSObjectStorage::removeObject(const StoredObject & object)
{
    const auto & path = object.remote_path;
    const size_t begin_of_path = path.find('/', path.find("//") + 2);

    /// Add path from root to file name
    int res = hdfsDelete(hdfs_fs.get(), path.substr(begin_of_path).c_str(), 0);
    if (res == -1)
        throw Exception(ErrorCodes::HDFS_ERROR, "HDFSDelete failed with path: {}", path);

}

void HDFSObjectStorage::removeObjects(const StoredObjects & objects)
{
    for (const auto & object : objects)
        removeObject(object);
}

void HDFSObjectStorage::removeObjectIfExists(const StoredObject & object)
{
    if (exists(object))
        removeObject(object);
}

void HDFSObjectStorage::removeObjectsIfExist(const StoredObjects & objects)
{
    for (const auto & object : objects)
        removeObjectIfExists(object);
}

ObjectMetadata HDFSObjectStorage::getObjectMetadata(const std::string & path) const
{
    auto * file_info = hdfsGetPathInfo(hdfs_fs.get(), path.data());
    if (!file_info)
        throw Exception(ErrorCodes::HDFS_ERROR,
                        "Cannot get file info for: {}. Error: {}", path, hdfsGetLastError());

    ObjectMetadata metadata;
    metadata.size_bytes = static_cast<size_t>(file_info->mSize);
    metadata.last_modified = file_info->mLastMod;

    hdfsFreeFileInfo(file_info, 1);
    return metadata;
}

void HDFSObjectStorage::listObjects(const std::string & path, RelativePathsWithMetadata & children, size_t max_keys) const
{
    auto * log = &Poco::Logger::get("HDFSObjectStorage");
    LOG_TRACE(log, "Trying to list files for {}", path);

    HDFSFileInfo ls;
    ls.file_info = hdfsListDirectory(hdfs_fs.get(), path.data(), &ls.length);

    if (ls.file_info == nullptr && errno != ENOENT) // NOLINT
    {
        // ignore file not found exception, keep throw other exception,
        // libhdfs3 doesn't have function to get exception type, so use errno.
        throw Exception(ErrorCodes::ACCESS_DENIED, "Cannot list directory {}: {}",
                        path, String(hdfsGetLastError()));
    }

    if (!ls.file_info && ls.length > 0)
    {
        throw Exception(ErrorCodes::LOGICAL_ERROR, "file_info shouldn't be null");
    }

    LOG_TRACE(log, "Listed {} files for {}", ls.length, path);

    for (int i = 0; i < ls.length; ++i)
    {
        const String file_path = fs::path(ls.file_info[i].mName).lexically_normal();
        const size_t last_slash = file_path.rfind('/');
        const String file_name = file_path.substr(last_slash);

        const bool is_directory = ls.file_info[i].mKind == 'D';
        if (is_directory)
        {
            listObjects(fs::path(file_path) / "", children, max_keys);
        }
        else
        {
            LOG_TEST(log, "Found file: {}", file_path);

            children.emplace_back(std::make_shared<RelativePathWithMetadata>(
                String(file_path),
                ObjectMetadata{
                    static_cast<uint64_t>(ls.file_info[i].mSize),
                    Poco::Timestamp::fromEpochTime(ls.file_info[i].mLastMod),
                    {}}));
        }
    }
}

void HDFSObjectStorage::copyObject( /// NOLINT
    const StoredObject & object_from,
    const StoredObject & object_to,
    const ReadSettings & read_settings,
    const WriteSettings & write_settings,
    std::optional<ObjectAttributes> object_to_attributes)
{
    if (object_to_attributes.has_value())
        throw Exception(
            ErrorCodes::UNSUPPORTED_METHOD,
            "HDFS API doesn't support custom attributes/metadata for stored objects");

    auto in = readObject(object_from, read_settings);
    auto out = writeObject(object_to, WriteMode::Rewrite, /* attributes= */ {}, /* buf_size= */ DBMS_DEFAULT_BUFFER_SIZE, write_settings);
    copyData(*in, *out);
    out->finalize();
}


std::unique_ptr<IObjectStorage> HDFSObjectStorage::cloneObjectStorage(
    const std::string &,
    const Poco::Util::AbstractConfiguration &,
    const std::string &, ContextPtr)
{
    throw Exception(ErrorCodes::UNSUPPORTED_METHOD, "HDFS object storage doesn't support cloning");
}

}

#endif
