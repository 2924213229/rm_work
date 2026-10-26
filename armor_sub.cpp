#include <opencv2/opencv.hpp>
#include <opencv2/core/core.hpp>
#include <opencv2/highgui/highgui.hpp>
#include <iostream>
#include <zenoh.hxx>
#include <thread>
#include <mutex>
#include <condition_variable>
#include <chrono>

float distance(cv::Point2f p1, cv::Point2f p2){
        return sqrt((p1.x-p2.x)*(p1.x-p2.x)+(p1.y-p2.y)*(p1.y-p2.y));
}

class LightDescriptor{
public:float width, length, angle, area;
        cv::Point2f center;
        cv::Point2f corner[4];
public:
        LightDescriptor(){};
        LightDescriptor(const cv:: RotatedRect& light){
                width = std::min(light.size.width, light.size.height);
                length = std::max(light.size.width, light.size.height);
                angle = light.angle;
                area = light.size.area();
                center = light.center;
                light.points(corner);
        };
};

std::uint64_t read_uint64(std::uint8_t* p){
		std::uint64_t res = 0;
		for (int i = 0; i < 8; ++i){
			res |= static_cast<std::uint64_t>(p[i]) << (i * 8);
		}
		return res;
}

std::uint32_t read_uint32(std::uint8_t* p){
        std::uint32_t res = 0;
        for (int i = 0; i < 4; ++i){
                res |= static_cast<std::uint32_t>(p[i]) << (i * 8);
        }
        return res;
}


void detect_armors(cv::Mat bgr, cv::Mat& result){
        int cddt = 0;
        cv::Mat gray, bin;
        cv::cvtColor(bgr, gray, cv::COLOR_BGR2GRAY);
        cv::threshold(gray, bin, 90, 255, cv::THRESH_BINARY);
        std::vector<std::vector<cv::Point>> contours;
        cv::findContours(bin, contours, cv::RETR_EXTERNAL, cv::CHAIN_APPROX_NONE);
        std::vector<LightDescriptor> light;
        result = bgr.clone();
        for (const auto& contour : contours){
                cv::RotatedRect rect = cv::minAreaRect(contour);
                light.push_back(rect);
        }
        for (int i = 0; i <= light.size()-2; i++){
                for (int j = i + 1; j<= light.size() - 1; j++){
                        LightDescriptor& leftLight = light[i];
                        LightDescriptor& rightLight = light[j];
                        float angleGap_ = abs(leftLight.angle - rightLight.angle);
                        //由于灯条长度会因为远近而受到影响，所以按照比值去匹配灯条
                        float LenGap_ratio = abs(leftLight.length - rightLight.length) / std::max(leftLight.length, rightLight.length);
                        float dis = distance(leftLight.center, rightLight.center);
                        //均长
                        float meanLen = (leftLight.length + rightLight.length) / 2;
                        float lengap_ratio = abs(leftLight.length - rightLight.length) / meanLen;
                        float ratio = dis / meanLen;
                        float light_length_bi = leftLight.length / rightLight.length;
                        //匹配不通过的条件
                        if (angleGap_ > 45 ||
                                LenGap_ratio >= 0.5 ||
                                lengap_ratio >= 0.4 ||
                                dis <= 1||
                                light_length_bi >= 1.3 || light_length_bi <= 1/1.3) continue;
                        cddt++;
			LightDescriptor* leftBar = &light[i], *rightBar = &light[j];
			if (leftBar->center.x > rightBar->center.x) std::swap(leftBar, rightBar);
			cv::Point2f u = (rightBar->center - leftBar->center) / dis;
			cv::Point2f v(-u.y, u.x);
			if (v.y < 0) v = -v;
			cv::Point2f center = (leftBar->center + rightBar->center) * 0.5f;
			auto endMidpoint = [&](const LightDescriptor& bar, bool top) {
				float along[4];
				float minAlong = (bar.corner[0] - bar.center).dot(v), maxAlong = minAlong;
				for (int k = 0; k < 4; ++k) {
				        cv::Point2f offset = bar.corner[k] - bar.center;
				        along[k] = offset.dot(v);
				        minAlong = std::min(minAlong, along[k]);
				        maxAlong = std::max(maxAlong, along[k]);
				}
				float middle = (minAlong + maxAlong) * 0.5f;
			        cv::Point2f midpoint(0, 0);
				int count = 0;
				for (int k = 0; k < 4; ++k) {
				if (top ? along[k] <= middle : along[k] > middle) {
					midpoint += bar.corner[k];
					++count;
				        }
			        }
				return count ? midpoint * (1.0f / count) : bar.center;
							};
				cv::Point2f corners[4] = {
				endMidpoint(*leftBar, true), endMidpoint(*rightBar, true),
				endMidpoint(*rightBar, false), endMidpoint(*leftBar, false)};
				cv::Point vertices[4];
				cv::Point2f textloc(center.x - 100, center.y - 100);
				for (int k = 0; k < 4; ++k) vertices[k] = cv::Point(cvRound(corners[k].x), cvRound(corners[k].y));
				for (int k = 0; k < 4; ++k) cv::line(result, vertices[k], vertices[(k + 1) % 4], cv::Scalar(0, 0, 255), 2);
				cv::circle(result, cv::Point(cvRound(center.x), cvRound(center.y)), 5, cv::Scalar(0, 255, 0), 2, 8, 0);
                        std::string text = "condidate:" + std::to_string(cddt);
                        cv::putText(result, text, textloc, cv::FONT_HERSHEY_COMPLEX, 1, cv::Scalar(255, 0, 255));
                }
        }

        
}

std::mutex frame_mutex; 
std::condition_variable frame_cv; 
bool frame_ready = 0, stopping = 0;
std::uint64_t new_seq = 0; 
std::shared_ptr<const zenoh::ZShm> new_shm; 
cv::Mat new_frame, detect_img;

int main(){
        cv::Mat frame;
        std::thread t([&](){
                while (true){
                        cv::Mat img; 
                        std::shared_ptr<const zenoh::ZShm> shm; 
                        bool ready = 0;
                        {
                                std::unique_lock<std::mutex> lock(frame_mutex); 
                                frame_cv.wait_for(lock, std::chrono::milliseconds(10), []{ return frame_ready || stopping; });
                                if (stopping) return; 
                                if (frame_ready) { 
                                        img = std::move(new_frame); 
                                        shm = std::move(new_shm); 
                                        frame_ready = 0; 
                                        ready = true; 
                                }
                        }
                        if (ready) { 
                                detect_armors(img, detect_img); 
                                cv::imshow("raw", img);
                                cv::imshow("detect", detect_img); 
                                }
                        if ((cv::waitKey(1) & 0xff) == 'q') { 
                                std::lock_guard<std::mutex> lock(frame_mutex); 
                                stopping = true; 
                                frame_cv.notify_one(); 
                                return; 
                        }
                }
        });
        auto config = zenoh::Config::create_default();
        config.insert_json5("transport/shared_memory/enabled", "true");
        auto session = zenoh::Session::open(std::move(config));
        auto subscriber = session.declare_subscriber(zenoh::KeyExpr("work/msg"),
        [](zenoh::Sample& sample){
                auto shm_cor = sample.get_payload().as_shm();
                std::cout << typeid(shm_cor).name() << std::endl;
                if (shm_cor.has_value()){
                        std::cout << "shm successful" << std::endl;
                }

                auto* bytes = const_cast<std::uint8_t*>(shm_cor->get().data());
                auto seq = read_uint64(bytes);
                auto time = read_uint64(bytes + 8);
                auto width = read_uint32(bytes + 16);
                auto height = read_uint32(bytes + 20);
                const std::size_t frame_size = std::size_t(width) * height * 3, payload_size = shm_cor->get().len();
                
                std::cout << "seq:" << seq << std::endl;
                std::cout << "time:" << time << std::endl;
                std::cout << "width:" << width << std::endl;
                std::cout << "height:" << height << std::endl;

                cv::Mat frame(height, width, CV_8UC3, bytes + 24);
                {
                        std::lock_guard<std::mutex> lock(frame_mutex);
                        new_shm = std::make_shared<zenoh::ZShm>(shm_cor->get()); 
                        new_frame = frame; 
                        new_seq = std::max(seq, new_seq);
                        if (new_seq != seq) return;
                        frame_ready = 1; 
                }
                frame_cv.notify_one();
        },[](){});
        t.join();
        return 0;
}
