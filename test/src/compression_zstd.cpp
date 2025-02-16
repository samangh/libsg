#include <sg/compression_zstd.h>

#include <catch2/catch_test_macros.hpp>

TEST_CASE("sg::compression::zstd check byteswap(...) family", "[sg::compression::zstd]") {
    std::vector<int> in{123,456,789};

    auto comp = sg::compression::zstd::compress((void*)in.data(), sizeof(int)* in.size(), 3, 1);

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
