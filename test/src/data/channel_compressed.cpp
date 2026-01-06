#include "sg/data/channel_compressed.h"
#include "sg/data/channel_vector.h"

#include <catch2/catch_test_macros.hpp>


TEST_CASE("sg::data: channel_compressed: check constructor()", "[sg::data]") {
    auto a = sg::data::compressed_channel<sg::data::vector_channel<int>>();
}