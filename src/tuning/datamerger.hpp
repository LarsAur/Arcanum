#pragma once

#include <vector>
#include <string>

namespace Arcanum
{
    class DataMerger
    {
        private:
            std::vector<std::string> m_inputPaths;
            std::string m_outputPath;
        public:
        DataMerger() = default;
        ~DataMerger() = default;

        void addInputPath(const std::string& path);
        void setOutputPath(const std::string& path);
        bool mergeData();
    };
}