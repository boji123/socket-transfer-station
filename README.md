# socket-transfer-station

两年前写的一个程序，比较简陋，传上来备忘。

这是一个基于C++写的linux服务器中转示例，可以将语音数据从本地发送到服务器，然后由服务器中转到讯飞语音接口进行识别，然后再中转回来。

写这个程序，一个是为了熟悉socket服务器的运作，另一个是可以将原本单通道的讯飞接口改造成分时多通道，并且后续可以扩展数据分析等功能。

包含一个讯飞的include包，一个client文件，一个server文件，以及数个用于测试的语音文件。其中client文件用于模拟本地的语音数据发送，server文件应当在服务器上后台运行接收数据。


- 使用前请先在server.cpp中填入自己的讯飞appid，然后将讯飞SDK里的MSC包替换include文件夹中的对应文件。
- 运行build.sh进行编译
```
./build.sh
```
- 该程序会对client.cpp和server.cpp进行编译，并自动生成一个包含server运行所需文件的pack
- 在本地调试的时候，先后台启动server
```
nohup ./server >> server.log
```
- 调用client将测试语音本地发送到server
```
./client wav/9_6_0_1_2_5_4_3_8_7.wav
```
- 调用成功会收到返回信息，受系统编码问题，显示可能不正常，请自行修改
- 可以查看server.log得到服务器的运行和识别过程
- 更改server.cpp里的IP并重新编译，可以监听所需端口

