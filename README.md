#zenoh收发视频帧识别装甲板
**version**:
zenoh:1.10.1;
opencv:4.60;
cpp:17;
cmake:3.16;
**build**:
cmake -S . -B build
cmake --build build --target armor_sub video_pub
**run**:
first:./armor_sub
second:./video_sub
按q退出
**frame_protocal**
seq(8bytes) + time(8bytes) + width(4bytes) + height(4bytes)小端序
**筛选参数**
二值化下界90上界255
两灯条角差值小于45度
两灯条长度差值绝对值比两灯条长度最大值小于0.5
两灯条长度差值绝对值比两灯条长度平均值小于0.4
两灯条中心距离大于1

**0copy**
使用as_shm()获取信息,判断shm有值输出shm successful，以及使用指针构建图像
