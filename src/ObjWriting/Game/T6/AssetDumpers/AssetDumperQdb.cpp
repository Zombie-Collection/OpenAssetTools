#include "AssetDumperQdb.h"

using namespace T6;

std::string AssetDumperQdb::GetFileNameForAsset(Zone* zone, Qdb* asset)
{
    return std::string(asset->name);
}

void AssetDumperQdb::DumpAsset(Zone* zone, Qdb* asset, FileAPI::File* out)
{
    out->Write(asset->buffer, 1, asset->len);
}