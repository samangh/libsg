#include "sg/rolling_contiguous_buffer.h"

#include <atomic>
#include <catch2/catch_test_macros.hpp>

TEST_CASE("sg::common rolling_contiguous_buffer: check size() and capacity()",
          "[sg::rolling_contiguous_buffer]") {
    sg::rolling_contiguous_buffer<int> buffer(5);

    /* check size() */
    REQUIRE(buffer.size()== 0);
    buffer.push_back(0);
    REQUIRE(buffer.size()== 1);

    /* check capacity */
    REQUIRE(buffer.capacity()== 5);
}

TEST_CASE("sg::common rolling_contiguous_buffer: check push_back()",
          "[sg::rolling_contiguous_buffer]") {
    sg::rolling_contiguous_buffer<int> buffer(5);

    auto count=0;

    for (size_t i=0; i <5; i++)
        buffer.push_back(1);

    for (size_t i=0; i <5; i++)
        count += buffer[i];

    REQUIRE(count== 5);
}

TEST_CASE("sg::common rolling_contiguous_buffer: check append(...)",
          "[sg::rolling_contiguous_buffer]") {

    /* with iterator */
    {
        sg::rolling_contiguous_buffer<int> buffer(5);
        std::vector<int> v = {0, 1, 2, 3, 4};

        buffer.append(v.begin(), v.end());
        REQUIRE(buffer[0] == 0);
        REQUIRE(buffer[4] == 4);

        v = {5, 6, 7, 8, 9};
        buffer.append(v.begin(), v.end());
        REQUIRE(buffer[0] == 5);
        REQUIRE(buffer[4] == 9);

        v= {0, 1, 2, 3, 4, 5};
        buffer.append(v.begin(), v.end());
        REQUIRE(buffer[0] == 1);
        REQUIRE(buffer[4] == 5);
    }

    /* with initializer_list */
    {
        sg::rolling_contiguous_buffer<int> buffer(5);
        std::vector<int> v = {0, 1, 2, 3, 4};

        buffer.append(v.begin(), v.end());
        REQUIRE(buffer[0] == 0);
        REQUIRE(buffer[4] == 4);

        v = {5, 6, 7, 8, 9};
        buffer.append(v.begin(), v.end());
        REQUIRE(buffer[0] == 5);
        REQUIRE(buffer[4] == 9);

        v= {0, 1, 2, 3, 4, 5};
        buffer.append(v.begin(), v.end());
        REQUIRE(buffer[0] == 1);
        REQUIRE(buffer[4] == 5);
    }

    /* with initializer_list */
    {
        sg::rolling_contiguous_buffer<int> buffer(5);

        buffer.append({0, 1, 2, 3, 4});
        REQUIRE(buffer[0] == 0);
        REQUIRE(buffer[4] == 4);

        buffer.append({5, 6, 7, 8, 9});
        REQUIRE(buffer[0] == 5);
        REQUIRE(buffer[4] == 9);

        buffer.append({0, 1, 2, 3, 4, 5});
        REQUIRE(buffer[0] == 1);
        REQUIRE(buffer[4] == 5);
    }
}



TEST_CASE("sg::common rolling_contiguous_buffer: check rolling works before memcpy()",
          "[sg::rolling_contiguous_buffer]") {
    sg::rolling_contiguous_buffer<int> buffer(5);
    auto count=0;

    /* fill full size */
    for (size_t i=0; i <5; i++)
        buffer.push_back(0);

    /* full full size again== push all of old values out*/
    for (size_t i=0; i <5; i++)
        buffer.push_back(1);

    for (size_t i=0; i <5; i++)
        count += buffer[i];

    REQUIRE(count== 5);
    REQUIRE(buffer.size()== 5);
}

TEST_CASE("sg::common rolling_contiguous_buffer: check rolling works after memcpy()",
          "[sg::rolling_contiguous_buffer]") {
    sg::rolling_contiguous_buffer<int> buffer(5);
    auto count=0;

    /* fill full twice over */
    for (size_t i=0; i <10; i++)
        buffer.push_back(0);

    /* full full size again, causing a memcpy()*/
    for (size_t i=0; i <5; i++)
        buffer.push_back(1);

    for (size_t i=0; i <5; i++)
        count += buffer[i];

    REQUIRE(count== 5);
    REQUIRE(buffer.size()== 5);
}

TEST_CASE("sg::common rolling_contiguous_buffer: check append() past total allocation",
          "[sg::rolling_contiguous_buffer]") {

    /* memcpy path: trivially-copyable T with reserve >= size.
     *
     * A bulk append can jump pos_end straight past the total allocation, so the shift must
     * trigger on crossing the boundary, not on landing exactly at it. */
    {
        sg::rolling_contiguous_buffer<int> buffer(5);

        /* 9 single pushes: one short of the total allocation (5+5) */
        for (int i = 0; i < 9; i++)
            buffer.push_back(i);

        /* jumps from 9 to 12, past the allocation of 10 */
        buffer.append({9, 10, 11});

        REQUIRE(buffer.size() == 5);
        for (int i = 0; i < 5; i++)
            REQUIRE(buffer[i] == 7 + i);
    }

    /* move path: reserve < size, so the shift goes through ensure_space_move() */
    {
        sg::rolling_contiguous_buffer<int> buffer(10, 2);

        for (int i = 0; i < 11; i++)
            buffer.push_back(i);

        /* jumps from 11 to 14, past the allocation of 12 */
        buffer.append({11, 12, 13});

        REQUIRE(buffer.size() == 10);
        for (int i = 0; i < 10; i++)
            REQUIRE(buffer[i] == 4 + i);
    }

    /* shift triggered before the window has ever filled */
    {
        sg::rolling_contiguous_buffer<int> buffer(10, 2);

        for (int i = 0; i < 9; i++)
            buffer.push_back(i);

        /* jumps from 9 to 14, past the allocation of 12, while only 9 of 10 slots are used */
        buffer.append({9, 10, 11, 12, 13});

        REQUIRE(buffer.size() == 10);
        for (int i = 0; i < 10; i++)
            REQUIRE(buffer[i] == 4 + i);
    }

    /* repeated boundary-jumping appends keep the buffer consistent */
    {
        sg::rolling_contiguous_buffer<int> buffer(5);

        int next = 0;
        for (int i = 0; i < 100; i++) {
            buffer.append({next, next + 1, next + 2});
            next += 3;
        }

        REQUIRE(buffer.size() == 5);
        for (int i = 0; i < 5; i++)
            REQUIRE(buffer[i] == next - 5 + i);
    }
}

TEST_CASE("sg::common rolling_contiguous_buffer: check resize() growth",
          "[sg::rolling_contiguous_buffer]") {
    sg::rolling_contiguous_buffer<int> buffer(5);
    auto                               count = 0;

    /* fill fully */
    for (size_t i = 0; i < 10; i++)
        buffer.push_back(1);

    for (size_t i = 0; i < buffer.size(); i++)
        count += buffer[i];

    REQUIRE(count== 5);
    REQUIRE(buffer.size()== 5);

    buffer.resize(10);
    REQUIRE(buffer.size()== 5);

    /* fill fully */
    for (size_t i = 5; i < 10; i++)
        buffer.push_back(1);

    count = 0;
    for (size_t i = 0; i < buffer.size(); i++)
        count += buffer[i];

    REQUIRE(count== 10);
    REQUIRE(buffer.size()== 10);
}

TEST_CASE("sg::common rolling_contiguous_buffer: check resize() reduction",
          "[sg::rolling_contiguous_buffer]") {
    sg::rolling_contiguous_buffer<int> buffer(5);
    auto                               count = 0;

    /* fill fully */
    for (size_t i = 0; i < 5; i++)
        buffer.push_back(i);

    buffer.resize(2);
    REQUIRE(buffer.size()== 2);
    REQUIRE(buffer.capacity()== 2);

    for (size_t i = 0; i < buffer.size(); i++)
        count += buffer[i];

    REQUIRE(count== 3+4);
}

TEST_CASE("sg::common rolling_contiguous_buffer: check for loop iteration",
          "[sg::rolling_contiguous_buffer]") {
    sg::rolling_contiguous_buffer<int> buffer(5);
    auto                               count = 0;

    /* fill fully */
    for (size_t i = 0; i < 5; i++)
        buffer.push_back(1);

    count=0;
    for (auto& a: buffer)
        count +=a;
    REQUIRE(count== 5);

    count=0;
    for (auto a: buffer)
        count +=a;
    REQUIRE(count== 5);

    count=0;
    for (const auto a: buffer)
        count +=a;
    REQUIRE(count== 5);

    count=0;
    for (const auto& a: buffer)
        count +=a;
    REQUIRE(count== 5);
}

TEST_CASE("sg::common rolling_contiguous_buffer: check begin/end",
          "[sg::rolling_contiguous_buffer]") {
    sg::rolling_contiguous_buffer<int> buffer(5);

    /* fill fully */
    for (size_t i = 0; i < 5; i++)
        buffer.push_back(1);

    REQUIRE(buffer.begin()== &buffer[0]);
    REQUIRE(buffer.end()== &buffer[4]+1);

    REQUIRE(buffer.cbegin()== &buffer[0]);
    REQUIRE(buffer.cend()== &buffer[4]+1);

    int counter=0;
    *buffer.begin()=2;
    for(auto it=buffer.begin(); it !=buffer.end(); ++it)
        counter += *it;
    REQUIRE(counter== 6);
}

TEST_CASE("sg::common rolling_contiguous_buffer: modifying through []",
          "[sg::rolling_contiguous_buffer]") {
    sg::rolling_contiguous_buffer<int> buffer(5);

    /* fill fully */
    for (size_t i = 0; i < 5; i++)
        buffer.push_back(1);

    buffer[1]=2;
    REQUIRE(buffer[1]== 2);
}

TEST_CASE("sg::common rolling_contiguous_buffer: check copy/move",
          "[sg::rolling_contiguous_buffer]") {
    std::atomic_int ctr_count = 0;
    std::atomic_int dest_count = 0;
    std::atomic_int copy_count = 0;

    /* test class to count copies and moves */
    class test {
        bool             has_data{true};
        std::atomic_int* dest_counter;
        std::atomic_int* copy_counter;
        std::atomic_int* ctr_counter;
    public:
        test(std::atomic_int* dest_counter, std::atomic_int* copy_counter, std::atomic_int* ctr_counter)
            : dest_counter(dest_counter),
              copy_counter(copy_counter),
              ctr_counter(ctr_counter)
        {
            ctr_counter->fetch_add(1);
        }

        test(const test& other)
            : dest_counter(other.dest_counter),
              copy_counter(other.copy_counter),
              ctr_counter(other.ctr_counter) {
            copy_counter->fetch_add(1);
        }

        test& operator=(const test& other) {
            dest_counter = other.dest_counter;
            copy_counter = other.copy_counter;
            ctr_counter = other.ctr_counter;
            copy_counter->fetch_add(1);
            return *this;
        }

        /* move constructors */
        test(test&& other) noexcept
            : dest_counter(other.dest_counter),
              copy_counter(other.copy_counter),
              ctr_counter(other.ctr_counter) {
            other.has_data = false;
            has_data = true;
        }

        test& operator=(test&& other) noexcept {
            if (this != &other) {
                dest_counter = other.dest_counter;
                copy_counter = other.copy_counter;
                ctr_counter = other.ctr_counter;
                other.has_data = false;
                has_data = true;
            }
            return *this;
        }

        ~test() {
            if (has_data)
                dest_counter->fetch_add(1);
        }
    };

    {
        sg::rolling_contiguous_buffer<test> buffer(5);
        for (int i = 0; i < 5; i++)
            buffer.emplace_back(test(&dest_count, &copy_count, &ctr_count));
    }

    /* check that data was actually cleared only 5 times */
    REQUIRE(ctr_count== 5);
    REQUIRE(dest_count== 5);
    REQUIRE(copy_count== 0);
}
