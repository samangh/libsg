#include "sg/rolling_contiguous_buffer.h"

#include <atomic>
#include <doctest/doctest.h>

TEST_CASE("SG::common rolling_contiguous_buffer: check size() and capacity()") {
    sg::rolling_contiguous_buffer<int> buffer(5);

    /* check size() */
    CHECK_EQ(buffer.size(), 0);
    buffer.push_back(0);
    CHECK_EQ(buffer.size(), 1);

    /* check capacity */
    CHECK_EQ(buffer.capacity(), 5);
}

TEST_CASE("SG::common rolling_contiguous_buffer: check push_back()") {
    sg::rolling_contiguous_buffer<int> buffer(5);

    auto count=0;

    for (size_t i=0; i <5; i++)
        buffer.push_back(1);

    for (size_t i=0; i <5; i++)
        count += buffer[i];

    CHECK_EQ(count, 5);
}

TEST_CASE("SG::common rolling_contiguous_buffer: check rolling works before memcpy()") {
    sg::rolling_contiguous_buffer<int> buffer(5);
    auto count=0;

    /* fill full size */
    for (size_t i=0; i <5; i++)
        buffer.push_back(0);

    /* full full size again, push all of old values out*/
    for (size_t i=0; i <5; i++)
        buffer.push_back(1);

    for (size_t i=0; i <5; i++)
        count += buffer[i];

    CHECK_EQ(count, 5);
    CHECK_EQ(buffer.size(), 5);
}

TEST_CASE("SG::common rolling_contiguous_buffer: check rolling works after memcpy()") {
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

    CHECK_EQ(count, 5);
    CHECK_EQ(buffer.size(), 5);
}

TEST_CASE("SG::common rolling_contiguous_buffer: check resize() growth") {
    sg::rolling_contiguous_buffer<int> buffer(5);
    auto                               count = 0;

    /* fill fully */
    for (size_t i = 0; i < 10; i++)
        buffer.push_back(1);

    for (size_t i = 0; i < buffer.size(); i++)
        count += buffer[i];

    CHECK_EQ(count, 5);
    CHECK_EQ(buffer.size(), 5);

    buffer.resize(10);
    CHECK_EQ(buffer.size(), 5);

    /* fill fully */
    for (size_t i = 5; i < 10; i++)
        buffer.push_back(1);

    count = 0;
    for (size_t i = 0; i < buffer.size(); i++)
        count += buffer[i];

    CHECK_EQ(count, 10);
    CHECK_EQ(buffer.size(), 10);
}

TEST_CASE("SG::common rolling_contiguous_buffer: check resize() reduction") {
    sg::rolling_contiguous_buffer<int> buffer(5);
    auto                               count = 0;

    /* fill fully */
    for (size_t i = 0; i < 5; i++)
        buffer.push_back(i);

    buffer.resize(2);
    CHECK_EQ(buffer.size(), 2);
    CHECK_EQ(buffer.capacity(), 2);

    for (size_t i = 0; i < buffer.size(); i++)
        count += buffer[i];

    CHECK_EQ(count, 3+4);
}

TEST_CASE("SG::common rolling_contiguous_buffer: check for loop iteration") {
    sg::rolling_contiguous_buffer<int> buffer(5);
    auto                               count = 0;

    /* fill fully */
    for (size_t i = 0; i < 5; i++)
        buffer.push_back(1);

    count=0;
    for (auto& a: buffer)
        count +=a;
    CHECK_EQ(count, 5);

    count=0;
    for (auto a: buffer)
        count +=a;
    CHECK_EQ(count, 5);

    count=0;
    for (const auto a: buffer)
        count +=a;
    CHECK_EQ(count, 5);

    count=0;
    for (const auto& a: buffer)
        count +=a;
    CHECK_EQ(count, 5);
}

TEST_CASE("SG::common rolling_contiguous_buffer: check begin/end") {
    sg::rolling_contiguous_buffer<int> buffer(5);

    /* fill fully */
    for (size_t i = 0; i < 5; i++)
        buffer.push_back(1);

    CHECK_EQ(buffer.begin(), &buffer[0]);
    CHECK_EQ(buffer.end(), &buffer[5]);

    CHECK_EQ(buffer.cbegin(), &buffer[0]);
    CHECK_EQ(buffer.cend(), &buffer[5]);

    int counter=0;
    *buffer.begin()=2;
    for(auto it=buffer.begin(); it !=buffer.end(); ++it)
        counter += *it;
    CHECK_EQ(counter, 6);
}

TEST_CASE("SG::common rolling_contiguous_buffer: modifying through []") {
    sg::rolling_contiguous_buffer<int> buffer(5);

    /* fill fully */
    for (size_t i = 0; i < 5; i++)
        buffer.push_back(1);

    buffer[1]=2;
    CHECK_EQ(buffer[1], 2);
}

TEST_CASE("SG::common rolling_contiguous_buffer: check copy/move") {
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
            buffer.push_back(test(&dest_count, &copy_count, &ctr_count));
    }

    /* check that data was actually cleared only 5 times */
    CHECK_EQ(ctr_count, 5);
    CHECK_EQ(dest_count, 5);
    CHECK_EQ(copy_count, 0);
}
