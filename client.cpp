#include <stdio.h>
#include <string.h>
#include <string>
#include <vector>
#include <stdlib.h>
#include <unistd.h>
#include <arpa/inet.h>
#include <sys/socket.h>


//------------------------------------------------------------------------------------------------
const char* IP = "127.0.0.1";//server port
const int PORT = 1234;

const int BUFFER_SIZE = 4096;
//------------------------------------------------------------------------------------------------
class Client{
	private:
	int _iSock;
	public:
	Client(){
		//创建套接字
		_iSock = socket(AF_INET, SOCK_STREAM, 0);

		//向服务器（特定的IP和端口）发起请求
		struct sockaddr_in serv_addr;
		memset(&serv_addr, 0, sizeof(serv_addr));	//每个字节都用0填充
		serv_addr.sin_family = AF_INET;  //使用IPv4地址
		serv_addr.sin_addr.s_addr = inet_addr(IP);	//具体的IP地址
		serv_addr.sin_port = htons(PORT);  //端口
		if (connect(_iSock, (struct sockaddr*)&serv_addr, sizeof(serv_addr)) < 0) {
			printf("connect failed\n");
			exit(1);
		}
		else{
			printf("connect %s:%d success!\n",IP,PORT);
		}
	}
	bool sendData(const char* pBuffer, int iNeedSend){
		int iPos;
		int iLen;
		int iNextBatch;
		iPos=0;
		while(iPos < iNeedSend){
			iNextBatch = iNeedSend - iPos > BUFFER_SIZE ? BUFFER_SIZE : iNeedSend - iPos;
			//printf("iNextBatch : %d\n",iNextBatch);
			char buffer[] ="12345678";
			iLen = send(_iSock, pBuffer + iPos, iNextBatch, 0);
			//printf("iLen : %d\n",iLen);
			if(iLen < 0){
				printf("send %d bytes failed!\n",iNeedSend);
				return false;
			}
			iPos += iLen;
		}
		//printf("send %d bytes\n",iNeedSend);
		return true;
	}
	bool sendInt(int iSend){
		int iPos;
		int iLen;
		iPos=0;
		while(iPos < 4){
			iLen = send(_iSock, (char*)&iSend + iPos, 4 - iPos, 0);
			if(iLen < 0){
				printf("send int failed!\n");
				return false;
			}
			iPos += iLen;
		}
		//printf("send int : %d\n",iSend);
		return true;
	}
	//一个更加安全的数据获取方式，按参数needRecv强制接收指定数目的字节数，并装载进buffer中
	//若接收不到数据将休眠并按一定间隔重试，当重试超过一定次数后报错并关闭当前的clientSocket
	bool getData(char* pBuffer, int iNeedRecv) {
		int iPos = 0;
		int iNextBatch;
		int iLen;

		int iRetryTime = 0;
		int iRetryTimeLimit = 100;
		int iRetryTimeDelayMs = 50;

		while (iPos < iNeedRecv) {
			iNextBatch = iNeedRecv - iPos > BUFFER_SIZE ? BUFFER_SIZE : iNeedRecv - iPos;
			iLen = recv(_iSock, pBuffer + iPos, iNextBatch, 0);
			if (iLen < 0) {
				printf("Server Recieve Data Failed!\n");
				return false;
			}
			else if (iLen == 0) {
				if (iRetryTime++ == iRetryTimeLimit) {
					printf("Link Error ! Couldn't Get Required Bytes In Time Limit\n");
					return false;
				}
				usleep(iRetryTimeDelayMs * 1000);
			}
			else {
				iRetryTime = 0;
				iPos += iLen;
			}
		}
		//printf("Recv = %d Bytes\n", iPos);
		return true;
	}
	bool getCurrData(char* pBuffer, int iNeedRecv) {
		int iPos = 0;
		int iNextBatch;
		int iLen;

		while (iPos < iNeedRecv) {
			iNextBatch = iNeedRecv - iPos > BUFFER_SIZE ? BUFFER_SIZE : iNeedRecv - iPos;
			iLen = recv(_iSock, pBuffer + iPos, iNextBatch, 0);
			if (iLen < 0) {
				printf("Server Recieve Data Failed!\n");
				return false;
			}
			else if (iLen == 0) {
				return true;
			}
			else {
				iPos += iLen;
			}
		}
		//printf("Recv = %d Bytes\n", iPos);
		return true;
	}
	void closeClient() {
		if (close(_iSock) == 0) {
			printf("Close Server Success\n");
		}
		else {
			printf("Error ! Close Server Failed\n");
			exit(1);
		}
	}
};

void start_one_recognize(std::string file_path){
	Client client;
	//-----------------------------------------------------------------
	int conf=4;
	client.sendInt(conf);
	const char* pTmp = "\0\0\0\0\0\0\0\0";
	client.sendData(pTmp,conf);
	//---------------------
	FILE*			f_pcm						=	NULL;
	char*			p_pcm						=	NULL;
	int			pcm_count					=	0;
	int			pcm_size					=	0;
	int			read_size					=	0;
	
	f_pcm = fopen(file_path.c_str(), "rb");
	if (NULL == f_pcm) 
	{
		printf("\nopen [%s] failed! \n", file_path.c_str());
	}
	
	fseek(f_pcm, 0, SEEK_END);
	pcm_size = ftell(f_pcm); //获取音频文件大小 
	fseek(f_pcm, 0, SEEK_SET);		

	p_pcm = (char *)malloc(pcm_size);
	if (NULL == p_pcm)
	{
		printf("\nout of memory! \n");
	}

	read_size = fread((void *)p_pcm, 1, pcm_size, f_pcm); //读取音频文件内容
	if (read_size != pcm_size)
	{
		printf("\nread [%s] error!\n", file_path.c_str());
	}
	
	//-----------------------------------------------
	char* cBuffer = p_pcm;
	p_pcm = NULL;
	int iFrameSize = 4096;
	int iNeedSend = read_size;
	int iByteSended = 0;
	int nextFrameSize;
	
	while(iByteSended < iNeedSend){
		
		nextFrameSize = iNeedSend - iByteSended > iFrameSize ? iFrameSize : iNeedSend - iByteSended;
		client.sendInt(nextFrameSize);
		client.sendData(cBuffer + iByteSended,nextFrameSize);
		iByteSended += nextFrameSize;
		
		usleep(60*1000);
	}
	//send a zero frame size to tell the server end receiving
	client.sendInt(0);
	
	usleep(5000*1000);
	
	char* pBuffer = (char*)malloc(4096);
	client.getCurrData(pBuffer, 4096);
	//---------find the result char----------
	char* pResult;
	int count=0;
	for(int i=strlen(pBuffer);i>=0;i--){
		if(pBuffer[i]=='\n')
			count++;
		if(count==3)
		{
			pResult=pBuffer+i+1;
			break;
		}
	}
	std::string sResult = pResult;
	//sResult = sResult.substr(7,sResult.size()-7-13);
	//---------------------------------------
	printf("recv result of file '%s' :\n",file_path.c_str());
	printf("content:'%s'\n",sResult.c_str());
	free(pBuffer);
	free(cBuffer);
	client.closeClient();
	//printf("send over\n");
}
int main(int argc,char *argv[]){
	if(argc!=2)
	{
		printf("error : argc is %d\n",argc);
		return 0;
	}
	std::string file_perfix,file_name,file_path;
	//file_perfix = "wenzen_datapack/audio/";
	//file_name = "0912_device10/19487.bin";
	//file_perfix = argv[1];
	//file_name = argv[2];
	file_path = argv[1];
	//file_path = file_perfix + file_name;
	start_one_recognize(file_path);
	return 0;
}
