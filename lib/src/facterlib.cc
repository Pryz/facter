#include <facter/facterlib.h>
#include <facter/version.h>
#include <facter/facts/collection.hpp>
#include <facter/facts/value.hpp>
#include <facter/util/string.hpp>
#include <facter/logging/logging.hpp>
#include <log4cxx/logger.h>
#include <log4cxx/patternlayout.h>
#include <log4cxx/consoleappender.h>
#include <memory>
#include <vector>
#include <string>

using namespace std;
using namespace facter::util;
using namespace facter::facts;
using namespace facter::ruby;
using namespace log4cxx;

static unique_ptr<collection> g_facts;
static vector<string> g_custom_directories;
static vector<string> g_external_directories;

void configure_logging(LevelPtr level)
{
    // If no configuration file given, use default settings
    LayoutPtr layout = new PatternLayout("%d %-5p %c - %m%n");
    AppenderPtr appender = new ConsoleAppender(layout, "System.err");
    Logger::getRootLogger()->addAppender(appender);
    Logger::getRootLogger()->setLevel(level);

    // Configure the execution output logger
    auto logger = Logger::getLogger(LOG_ROOT_NAMESPACE "execution.output");
    logger->setAdditivity(false);
    layout = new PatternLayout("%m%n");
    appender = new ConsoleAppender(layout, "System.err");
    logger->addAppender(appender);
}

extern "C" {
    char const* get_facter_version()
    {
        return LIBFACTER_VERSION;
    }

    void load_facts(char const* names)
    {
        if (g_facts) {
            return;
        }

        // Configure for warning level if logging is not yet configured
        if (Logger::getRootLogger()->getAllAppenders().size() == 0) {
            configure_logging(Level::getWarn());
        }

        auto ruby = api::instance();
        if (ruby) {
            ruby->initialize();
        }

        g_facts.reset(new collection());
        g_facts->add_default_facts();

        // Add the external and custom facts
        g_facts->add_external_facts(g_external_directories);
        if (ruby) {
            g_facts->add_custom_facts(*ruby, g_custom_directories);
        }

        // Filter to just the requested facts
        if (names) {
            set<string> requested_facts;
            for (auto& name : split(names, ',')) {
                requested_facts.emplace(trim(to_lower(move(name))));
            }
            if (!requested_facts.empty()) {
                g_facts->filter(requested_facts);
            }
        }
    }

    void clear_facts()
    {
        if (!g_facts) {
            return;
        }
        g_facts.reset(nullptr);
    }

    void enumerate_facts(enumeration_callbacks* callbacks)
    {
        if (!g_facts || !callbacks) {
            return;
        }

        g_facts->each([&](string const& name, value const* val) {
            val->notify(name, callbacks);
            return true;
        });
    }

    bool get_fact_value(char const* name, enumeration_callbacks* callbacks)
    {
        if (!g_facts || !name || !callbacks) {
            return false;
        }

        // Get the fact
        string fact = trim(to_lower(name));
        auto val = (*g_facts)[fact];
        if (!val) {
            return false;
        }

        // Notify of the fact value
        val->notify(fact, callbacks);
        return true;
    }

    void add_search_paths(char const* directories, char const* separator)
    {
        if (!directories || !separator || !*separator) {
            return;
        }

        for (auto& directory : split(directories, *separator)) {
            g_custom_directories.emplace_back(move(directory));
        }
    }

    void enumerate_search_paths(void(*callback)(char const* path))
    {
        if (!callback) {
            return;
        }
        for (auto const& directory : g_custom_directories) {
            callback(directory.c_str());
        }
    }

    void clear_search_paths()
    {
        g_custom_directories.clear();
    }

    void add_external_search_paths(char const* directories, char const* separator)
    {
        if (!directories || !separator || !*separator) {
            return;
        }

        for (auto& directory : split(directories, *separator)) {
            g_external_directories.emplace_back(move(directory));
        }
    }

    void enumerate_external_search_paths(void(*callback)(char const* path))
    {
        if (!callback) {
            return;
        }
        for (auto const& directory : g_external_directories) {
            callback(directory.c_str());
        }
    }

    void clear_external_search_paths()
    {
        g_external_directories.clear();
    }
}
