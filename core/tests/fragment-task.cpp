#include <string>
#include <thread>

#include <catch/catch.hpp>
#include <microhttpd.h>
#include <zmq.hpp>

#include <oneseismic/transfer.hpp>
#include <oneseismic/tasks.hpp>

#include "mhttpd.hpp"
#include "core.pb.h"

using namespace Catch::Matchers;

namespace {

/*
 * A 2x2x2 fragment where each byte is encoded by its index
 */
const auto index_2x2x2 = std::vector< std::uint8_t > {
    0x00, 0x01, 0x02, 0x03,
    0x04, 0x05, 0x06, 0x07,
    0x08, 0x09, 0xA0, 0xA1,
    0xA2, 0xA3, 0xA4, 0xA5,
    0xA6, 0xA7, 0xA8, 0xA9,
    0xB0, 0xB1, 0xB2, 0xB3,
    0xB4, 0xB5, 0xB6, 0xB7,
};


int fragment_response(
        void*,
        struct MHD_Connection* connection,
        const char*,
        const char*,
        const char*,
        const char*,
        size_t*,
        void**) {


    auto* response = MHD_create_response_from_buffer(
            index_2x2x2.size(),
            (void*)index_2x2x2.data(),
            MHD_RESPMEM_MUST_COPY
    );

    auto ret = MHD_queue_response(connection, MHD_HTTP_OK, response);
    MHD_destroy_response(response);
    return ret;
}

}

std::string make_slice_request(int dim, int idx) {
    oneseismic::fetch_request req;
    req.set_root("root");
    req.set_guid("0d235a7138104e00c421e63f5e3261bf2dc3254b");

    auto* shape = req.mutable_shape();
    shape->set_dim0(2);
    shape->set_dim1(2);
    shape->set_dim2(2);

    auto* slice = req.mutable_slice();
    slice->set_dim(dim);
    slice->set_idx(idx);

    auto* id = req.add_ids();
    id->set_dim0(0);
    id->set_dim1(0);
    id->set_dim2(0);

    std::string msg;
    req.SerializeToString(&msg);
    return msg;
}

TEST_CASE("Fragment is sliced and pushed through", "[slice]") {
    zmq::context_t ctx;
    zmq::socket_t sock(ctx, ZMQ_REQ);
    sock.bind("inproc://queue");

    std::thread task([&ctx] {
        mhttpd httpd(fragment_response);
        loopback_cfg storage(httpd.port());
        one::transfer xfer(1, storage);

        zmq::socket_t sock(ctx, ZMQ_REP);
        sock.connect("inproc://queue");
        one::fragment_task ft;
        ft.run(xfer, sock, sock);
    });

    const auto apireq = make_slice_request(0, 0);
    zmq::message_t apimsg(apireq.data(), apireq.size());
    sock.send(apimsg, zmq::send_flags::none);

    zmq::message_t msg;
    sock.recv(msg, zmq::recv_flags::none);
    oneseismic::fetch_response res;
    task.join();
    const auto ok = res.ParseFromArray(msg.data(), msg.size());
    REQUIRE(ok);

    std::vector< float > expected(4);
    std::memcpy(expected.data(), index_2x2x2.data(), index_2x2x2.size());

    const auto& tiles = res.slice().tiles();
    CHECK(tiles.size() == 1);
    CHECK(tiles.Get(0).id().dim0() == 0);
    CHECK(tiles.Get(0).id().dim1() == 0);
    CHECK(tiles.Get(0).id().dim2() == 0);
    CHECK(tiles.Get(0).v().size() == 4);
    auto v = std::vector< float >(
        tiles.Get(0).v().begin(),
        tiles.Get(0).v().end()
    );
    CHECK_THAT(v, Equals(expected));
}
