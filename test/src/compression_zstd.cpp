#include <sg/compression_zstd.h>

#include <catch2/catch_test_macros.hpp>

void test_zstd(int level, int thread_count) {
    std::vector<int> in{123,456,789};

    auto comp = sg::compression::zstd::compress((void*)in.data(), sizeof(int)* in.size(), level, thread_count);

    /********* decomp using input buffer *********/
    // To int uffer
    {
        auto decomp = sg::compression::zstd::decompress<int>(comp);
        for (decltype(in)::size_type i = 0; i < in.size(); i++) REQUIRE(in[i] == decomp[i]);
    }
    // To std::byte buffer
    {
        auto decomp = sg::compression::zstd::decompress(comp);
        for (decltype(in)::size_type i = 0; i < in.size(); i++) REQUIRE(in[i] == ((int *)decomp.get())[i]);
    }

    /********* decomp using input pointers *********/
    // To int uffer
    {
        auto decomp = sg::compression::zstd::decompress<int>(comp.get(), comp.size());
        for (decltype(in)::size_type i = 0; i < in.size(); i++) REQUIRE(in[i] == decomp[i]);
    }
    // To std::byte buffer
    {
        auto decomp = sg::compression::zstd::decompress(comp.get(), comp.size());
        for (decltype(in)::size_type i = 0; i < in.size(); i++) REQUIRE(in[i] == ((int *)decomp.get())[i]);
    }

    /********* decomp using void pointers for everything *********/
    {
        auto decmp_size = sg::compression::zstd::get_uncompressed_size(comp.get(), comp.size());
        auto decmp_ptr = sg::make_unique_c_buffer<std::byte>(decmp_size);

        sg::compression::zstd::decompress(comp.get(), comp.size(), decmp_ptr.get(), decmp_size);
        for (decltype(in)::size_type i = 0; i < in.size(); i++)
            REQUIRE(in[i] == ((int *)decmp_ptr.get())[i]);
    }
}

TEST_CASE("sg::compression::zstd check compress(...) and decompress(...) family", "[sg::compression::zstd]") {
    // Test level 3 compression with 0=singlethreaded, 1=multithreaded with 1 thread, and 2 threads
    test_zstd(3, 0);

    /* multi-threaded libzstd is not enabled in static libzstd builds */
    if (sg::compression::zstd::bounds_nothread().second >=2) {
        test_zstd(3, 1);
        test_zstd(3, 2);
    }
}

TEST_CASE("sg::compression::zstd check utility functions", "[sg::compression::zstd]") {
    auto bounds_clevel = sg::compression::zstd::bounds_compression_level();
    auto default_level = sg::compression::zstd::default_compresssion_level();
    REQUIRE(default_level >= bounds_clevel.first);
    REQUIRE(default_level <=  bounds_clevel.second);

    auto bounds_nthread = sg::compression::zstd::bounds_nothread();
    REQUIRE(bounds_nthread.first==0);
    REQUIRE(bounds_nthread.second>=0);
}
