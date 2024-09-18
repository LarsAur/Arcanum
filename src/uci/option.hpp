#pragma once

#include <utils.hpp>
#include <string>
#include <algorithm>
#include <functional>

namespace Arcanum
{
    namespace Interface
    {
        class Option
        {
            protected:
                std::string m_name;
                std::function<void()> m_callback;
            public:
                Option(std::string name, std::function<void()> callback = {}) : m_name(name), m_callback(callback) {}

                bool matches(std::string name)
                {
                    return strEqCi(m_name, name);
                }
        };

        class SpinOption : public Option
        {
            private:
                int32_t m_def, m_min, m_max;
            public:
                int32_t value;
                SpinOption(std::string name, int32_t def, int32_t min, int32_t max, std::function<void()> callback = []{}) :
                    Option(name, callback),
                    m_def(def),
                    m_min(min),
                    m_max(max),
                    value(def)
                {};

                virtual void list()
                {
                    UCI_OUT("option name " << m_name << " type spin default " << m_def << " min " << m_min << " max " << m_max)
                }

                virtual void set(std::string str)
                {
                    value = std::clamp(std::stoi(str), m_min, m_max);
                    m_callback();
                }
        };

        class CheckOption : public Option
        {
            private:
                bool m_def;
            public:
                bool value;
                CheckOption(std::string name, bool def, std::function<void()> callback = []{}) :
                    Option(name, callback),
                    m_def(def),
                    value(def)
                {};

                virtual void list()
                {
                    UCI_OUT("option name " << m_name << " type check default " << (m_def ? "true" : "false"))
                }

                virtual void set(std::string str)
                {
                    std::transform(str.begin(), str.end(), str.begin(), [](unsigned char c){ return std::tolower(c); });
                    if(str == "true")
                        value = true;
                    else if(str == "false")
                        value = false;
                    else
                        return; // Do not call the callback if no match is found

                    m_callback();
                }
        };

        class ButtonOption : public Option
        {
            private:
            public:
                ButtonOption(std::string name, std::function<void()> callback = []{}) :
                    Option(name, callback)
                {};

                virtual void list()
                {
                    UCI_OUT("option name " << m_name << " type button")
                }

                virtual void set(std::string str)
                {
                    m_callback();
                }
        };

        class StringOption : public Option
        {
            private:
                std::string m_def;
            public:
                std::string value;
                StringOption(std::string name, std::string def, std::function<void()> callback = []{}) :
                    Option(name, callback),
                    m_def(def),
                    value(def)
                {};

                virtual void list()
                {
                    UCI_OUT("option name " << m_name << " type string default " << m_def)
                }

                virtual void set(std::string str)
                {
                    value = str;
                    m_callback();
                }
        };

        // TODO: Add combo option

    }
}