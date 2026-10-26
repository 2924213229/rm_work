#zenoh收发视频帧识别装甲板\n
**version**:\n
zenoh:1.10.1;\n
opencv:4.60;\n
cpp:17;\n
cmake:3.16;\n
**build**:\n
cmake -S . -B build\n
cmake --build build --target armor_sub video_pub\n
**run**:\n
first:./armor_sub\n
second:./video_sub\n
按q退出\n
**frame_protocal**\n
seq(8bytes) + time(8bytes) + width(4bytes) + height(4bytes)小端序\n
**筛选参数**\n
二值化下界90上界255\n
两灯条角差值小于45度\n
两灯条长度差值绝对值比两灯条长度最大值小于0.5\n
两灯条长度差值绝对值比两灯条长度平均值小于0.4\n
两灯条中心距离大于1\n

**0copy**
使用as_shm()获取信息,判断shm有值输出shm successful，以及使用指针构建图像
