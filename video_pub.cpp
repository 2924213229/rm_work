#include <zenoh.hxx>
#include <opencv2/opencv.hpp>
#include <opencv2/core/core.hpp>
#include<opencv2/highgui/highgui.hpp>
#include <chrono>
#include <iostream>
#include <thread>

std::size_t msghSize = 24;

void write_u32_le(std::uint8_t* dst, std::uint32_t value) {
        for (std::size_t i = 0; i < 4; ++i) {
                dst[i] = static_cast<std::uint8_t>(value >> (i * 8)); 
        }
}

void write_u64_le(std::uint8_t* dst, std::uint64_t value) {
        for (std::size_t i = 0; i < 8; ++i) {
                dst[i] = static_cast<std::uint8_t>(value >> (i * 8));
        }
}

void write_msg_header(std::uint8_t* dst,
                      std::uint64_t seq,
                      std::uint64_t timestamp_ns,
                      std::uint32_t width,
                      std::uint32_t height) {
        write_u64_le(dst + 0, seq);
        write_u64_le(dst + 8, timestamp_ns);
        write_u32_le(dst + 16, width);
        write_u32_le(dst + 20, height);
}

int main(){
        auto config = zenoh::Config::create_default();
        config.insert_json5("transport/shared_memory/enabled", "true");
        zenoh::PosixShmProvider provider(64*1024*1024);
        auto session = zenoh::Session::open(std::move(config));
        auto publisher = session.declare_publisher(zenoh::KeyExpr("work/msg"));
        cv::VideoCapture video("../raw.mp4");
        cv::Mat frame;
        std::uint64_t seq = 0;

        if (!video.isOpened()){
                std::cerr << "Failed to open that video\n";
                std::exit(EXIT_FAILURE);
        }

        constexpr auto frame_period = std::chrono::milliseconds(33);
        auto next_frame_time = std::chrono::steady_clock::now();
        while (video.read(frame)){
                if (!frame.empty() && frame.type() == CV_8UC3 && frame.isContinuous()) {
                        const std::uint64_t timestamp_ns = static_cast<std::uint64_t>(
                                std::chrono::duration_cast<std::chrono::nanoseconds>(
                                        std::chrono::steady_clock::now().time_since_epoch())
                                        .count());
                        const std::uint64_t frame_seq = seq++;
                        const std::uint32_t width = static_cast<std::uint32_t>(frame.cols);
                        const std::uint32_t height = static_cast<std::uint32_t>(frame.rows);
                        const std::size_t payload_size = std::size_t(width) * height * 3;
                        const std::size_t n = msghSize + payload_size;
                        auto allocation = provider.alloc_gc_defrag(n);
                        auto* shm = std::get_if<zenoh::ZShmMut>(&allocation);
                        if (shm && shm->len() >= n) {
                                write_msg_header(shm->data(), frame_seq, timestamp_ns, width, height);
                                std::memcpy(shm->data() + msghSize, frame.data, payload_size);
                                publisher.put(zenoh::Bytes(std::move(*shm)));
                        }
                }

                next_frame_time += frame_period;
                std::this_thread::sleep_until(next_frame_time);
        }
}
