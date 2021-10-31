#pragma once

#include <memory>
#include <string>
#include <vector>

#include "Domain/MenuFeatureLevel.h"
#include "Parsing/IParserLineStream.h"

class MenuFileReader
{
public:
    using include_callback_t = std::function<std::unique_ptr<std::istream>(const std::string& filename)>;
    
private:
    const MenuFeatureLevel m_feature_level;
    const std::string m_file_name;

    IParserLineStream* m_stream;
    std::vector<std::unique_ptr<IParserLineStream>> m_open_streams;

    bool OpenBaseStream(std::istream& stream, include_callback_t includeCallback);
    void SetupDefinesProxy();
    void SetupStreamProxies();

public:
    MenuFileReader(std::istream& stream, std::string fileName, MenuFeatureLevel featureLevel);
    MenuFileReader(std::istream& stream, std::string fileName, MenuFeatureLevel featureLevel, include_callback_t includeCallback);

    bool ReadMenuFile();
};
