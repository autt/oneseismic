#define CATCH_CONFIG_RUNNER
#include <catch/catch.hpp>

#include <curl/curl.h>
#include <spdlog/spdlog.h>

/*
 * Use own defined main, because that's catch2's way of allowing custom global
 * init [1]. spdlog is turned off because otherwise test output gets very
 * chatty, and not as useful.
 *
 * [1] https://github.com/catchorg/Catch2/blob/master/docs/own-main.md
 */
int main(int argc, char** argv) {
    spdlog::set_level(spdlog::level::off);
    curl_global_init(CURL_GLOBAL_ALL);
    return Catch::Session().run( argc, argv );
}
