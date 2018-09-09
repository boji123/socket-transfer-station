#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <unistd.h>
#include <arpa/inet.h>
#include <sys/socket.h>
#include <netinet/in.h>

#include <thread>

#include "include/qisr.h"
#include "include/msp_cmn.h"
#include "include/msp_errors.h"
//---------------------------------------------------------------------------------------------------------------
const char* IP = "127.0.0.1";
const int PORT = 1234;
const int BUFFER_SIZE = 4096;
const int LISTEN_QUEUE_SIZE = 20;

const char* login_params = "appid = **your xunfei appid**, work_dir = ."; // 登录参数，appid与msc库绑定,请勿随意改动

class Server {
private:
	static int _iServSock;
	int _iClntSock;
public:
//---------------------------------------------------------------------------------------
	Server(int iClntSock){
		_iClntSock = iClntSock;
	}
	static bool beginServer(){
		//创建套接字
		_iServSock = socket(AF_INET, SOCK_STREAM, IPPROTO_TCP);
		//将套接字和IP、端口绑定
		struct sockaddr_in serv_addr;
		memset(&serv_addr, 0, sizeof(serv_addr));	//每个字节都用0填充
		serv_addr.sin_family = AF_INET;  //使用IPv4地址
		serv_addr.sin_addr.s_addr = inet_addr(IP);	//具体的IP地址
		serv_addr.sin_port = htons(PORT);  //端口
		if (bind(_iServSock, (struct sockaddr*)&serv_addr, sizeof(serv_addr))) {
			printf("Server Bind Port: %d Failed!\n", PORT);
			fflush(stdout);
			return false;
		}

		//进入监听状态，等待用户发起请求
		if (listen(_iServSock, LISTEN_QUEUE_SIZE)) {
			printf("Server Listen Failed!\n");
			return false;
		}
		else{
			printf("----------Server start listening %s:%d----------\n",IP,PORT);
			fflush(stdout);
		}
		return true;
	}
	static bool closeServer() {
		if (close(_iServSock) == 0) {
			printf("Close Server Success\n");
			fflush(stdout);
			return true;
		}
		else {
			printf("Error ! Close Server Failed\n");
			fflush(stdout);
			return false;
		}
	}
	static bool acceptClient(int* iClntSock) {
		struct sockaddr_in clnt_addr;
		socklen_t clnt_addr_size = sizeof(clnt_addr);
		*iClntSock = accept(_iServSock, (struct sockaddr*)&clnt_addr, &clnt_addr_size);
		if (*iClntSock < 0) {
			printf("Client Accept Failed!\n");
			fflush(stdout);
			return false;
		}
		else{
			printf("-----Accept Client : %s-----\n",inet_ntoa(clnt_addr.sin_addr));
			fflush(stdout);
			return true;
		}
	}
//---------------------------------------------------------------------------------------

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
			iLen = recv(_iClntSock, pBuffer + iPos, iNextBatch, 0);
			if (iLen < 0) {
				printf("Server Recieve Data Failed!\n");
				fflush(stdout);
				return false;
			}
			else if (iLen == 0) {
				if (iRetryTime++ == iRetryTimeLimit) {
					printf("Link Error ! Couldn't Get Required Bytes In Time Limit\n");
					fflush(stdout);
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
	bool getInt(int* pResult){
		int iPos = 0;
		int iLen;
		
		int iRetryTime = 0;
		int iRetryTimeLimit = 100;
		int iRetryTimeDelayMs = 50;

		while (iPos < 4) {
			iLen = recv(_iClntSock, (char*)pResult + iPos, 4 - iPos, 0);
			if (iLen < 0) {
				printf("Server Recieve Int Failed!\n");
				fflush(stdout);
				return false;
			}
			else if (iLen == 0) {
				if (iRetryTime++ == iRetryTimeLimit) {
					printf("Link Error ! Couldn't Get Required Bytes In Time Limit\n");
					fflush(stdout);
					return false;
				}
				usleep(iRetryTimeDelayMs * 1000);
			}
			else {
				iRetryTime = 0;
				iPos += iLen;
			}
		}
		return true;
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
			iLen = send(_iClntSock, pBuffer + iPos, iNextBatch, 0);
			//printf("iLen : %d\n",iLen);
			if(iLen < 0){
				printf("send %d bytes failed!\n",iNeedSend);
				fflush(stdout);
				return false;
			}
			iPos += iLen;
		}
		//printf("send %d bytes\n",iNeedSend);
		return true;
	}
	void closeClient() {
		if (close(_iClntSock) == 0) {
			printf("-----Close Client Success-----\n");
			fflush(stdout);
		}
		else {
			printf("Error ! Close Client Failed\n");
			fflush(stdout);
			//exit(1);
		}
	}
	~Server(){
		//printf("calling ~Server()\n");
		closeClient();
	}
};
int Server::_iServSock = 0;
//---------------------------------------------------------------------------------------------------------------
const int iMaxSize = 10*1024*1024;//10MB
const int FRAME_LEN = 640;//size for 20ms wav data(16k,16bit) : 16000*16*0.020/8=640Bytes
const int HINTS_SIZE = 100;
class AsrHandler{
	private:
	char* _pBuffer;
	int _iBufferSize;
	int _iDataSize;
	
	//for ifly------------------------
	const char* _session_begin_params;
	const char* _session_id;
	const char* hints;
	bool _bIfFirstFrame;
	//--------------------------------
	public:
	bool init(){
		
		_pBuffer = (char*)malloc(iMaxSize);
		_iBufferSize = iMaxSize;
		_iDataSize = 0;
		
		_bIfFirstFrame = true;
		hints = "test";

		int errcode;
		_session_begin_params = "sub = iat, domain = iat, language = zh_cn, accent = mandarin, sample_rate = 16000, result_type = plain, result_encoding = utf8";
		_session_id = QISRSessionBegin(NULL, _session_begin_params, &errcode); //听写不需要语法，第一个参数为NULL
		
		if (MSP_SUCCESS != errcode)
		{
			printf("\nQISRSessionBegin failed! error code:%d\n", errcode);
			QISRSessionEnd(_session_id, hints);
			return false;
			//exit(1);
		}
		return true;
	}
	~AsrHandler(){
		//printf("calling ~AsrHandler\n");
		QISRSessionEnd(_session_id, hints);
		free(_pBuffer);
	}
//----------------------------------------------------------------------------------
	static bool login(){
		/* 用户登录 */
		int ret = MSPLogin(NULL, NULL, login_params); //第一个参数是用户名，第二个参数是密码，均传NULL即可，第三个参数是登录参数	
		if (MSP_SUCCESS != ret)
		{
			printf("MSPLogin failed , Error code %d.\n",ret);
			MSPLogout(); //退出登录
			return false;
		}
		return true;
	}
	static bool logout(){
		MSPLogout(); //退出登录
		return true;
	}
//----------------------------------------------------------------------------------
	bool storeData(char* pData,int iSize){
		if(_iDataSize + iSize > _iBufferSize){
			int iMB = iMaxSize/1024/1024;
			printf("out of memory limit ! current memory limit is : %d MB\n",iMB);
			return false;
		}
		memcpy(_pBuffer + _iDataSize,pData,iSize);
		_iDataSize += iSize;
		
		//printf("%d bytes has been stored\n",iSize);
		//printf("datasize is : %d bytes\n",_iDataSize);
		return true;
	}
	//send data to ifly
	bool sendData(char* pDataRecv,int* pSize){
		*pSize = 0;
		int len = 10 * FRAME_LEN; // 每次写入200ms音频(16k，16bit)：1帧音频20ms，10帧=200ms。16k采样率的16位音频，一帧的大小为640Byte
		if (_iDataSize < len){ //不够一帧不发
			return true;
		}
		int ret;
		int rec_stat;
		int ep_stat;
		int errcode;
		
		//printf("_iDataSize : %d\n",_iDataSize);
		printf("calling QISRAudioWrite\n");
		fflush(stdout);
		if(_bIfFirstFrame == true){
			ret = QISRAudioWrite(_session_id, (const void *)_pBuffer, len, MSP_AUDIO_SAMPLE_FIRST, &ep_stat, &rec_stat);
			_bIfFirstFrame = false;
		}
		else{
			ret = QISRAudioWrite(_session_id, (const void *)_pBuffer, len, MSP_AUDIO_SAMPLE_CONTINUE, &ep_stat, &rec_stat);
		}

		if (MSP_SUCCESS != ret)
		{
			printf("\nQISRAudioWrite failed! error code:%d\n", ret);
			return false;
		}

		if (MSP_REC_STATUS_SUCCESS == rec_stat) //已经有部分听写结果
		{
			const char *rslt = QISRGetResult(_session_id, &rec_stat, 0, &errcode);
			if (MSP_SUCCESS != errcode)
			{
				printf("\nQISRGetResult failed! error code: %d\n", errcode);
				return false;
			}
			if (NULL != rslt)
			{
				unsigned int rslt_len = strlen(rslt);
				*pSize = rslt_len;
				memcpy(pDataRecv,rslt,rslt_len);
			}
		}
//---------------------------------------------------------------------------
		memcpy(_pBuffer,_pBuffer + len,_iDataSize - len);
		_iDataSize -= len;
		return true;
	}
	bool finishSend(char* pDataRecv,int* pSize){
		*pSize = 0;
		int ret;
		int rec_stat;
		int ep_stat;
		int errcode;
		
		ret = QISRAudioWrite(_session_id, (const void *)_pBuffer, _iDataSize, MSP_AUDIO_SAMPLE_CONTINUE, &ep_stat, &rec_stat);
		
		errcode = QISRAudioWrite(_session_id, NULL, 0, MSP_AUDIO_SAMPLE_LAST, &ep_stat, &rec_stat);
		if (MSP_SUCCESS != errcode)
		{
			printf("\nQISRAudioWrite failed! error code:%d \n", errcode);
			QISRSessionEnd(_session_id, hints);
			return false;
		}

		while (MSP_REC_STATUS_COMPLETE != rec_stat) 
		{
			const char *rslt = QISRGetResult(_session_id, &rec_stat, 0, &errcode);
			if (MSP_SUCCESS != errcode)
			{
				printf("\nQISRGetResult failed, error code: %d\n", errcode);
				return false;
			}
			if (NULL != rslt)
			{
				unsigned int rslt_len = strlen(rslt);
				//strncat(rec_result, rslt, rslt_len);
				memcpy(pDataRecv,rslt,rslt_len);
				*pSize = rslt_len;
			}
			usleep(150*1000); //防止频繁占用CPU
		}
		QISRSessionEnd(_session_id, hints);
		return true;
	}
};
//---------------------------------------------------------------------------------------------------------------
void handleLink(int iClntSock){
	Server server(iClntSock);
	AsrHandler asrHandler;//init and get a session id
	if(asrHandler.init() == false){
		return;
	}
	int conf;
	if(server.getInt(&conf) == false){
		return;
	}
	else{
		printf("receive conf = %d\n", conf);
	}
	if(conf > 10 || conf < 0){
		printf("conf : %d out of range!\n", conf);
		return;
	}
	char* pBuffer = (char*)malloc(conf);
	if(server.getData(pBuffer,conf) == false){
		free(pBuffer);
		return;
	}
	else{
		printf("receive %d zero bytes due to conf\n", conf);
	}
	free(pBuffer);
	
	//-----getting data [iNextWavSize][data]-----
	char* pResultRecv = (char*)malloc(BUFFER_SIZE);
	int iResultLen = 0;
	int iNextWavSize;
	while(1){
		if(server.getInt(&iNextWavSize) == false){
			free(pResultRecv);
			return;
		}
		//printf("iNextWavSize = %d\n", iNextWavSize);
		if(iNextWavSize == 0){
			//printf("end data send\n");
			break;
		}
		printf("calling server.getData\n");
		fflush(stdout);
		char* pBuffer = (char*)malloc(iNextWavSize);
		if(server.getData(pBuffer,iNextWavSize) == false){
			free(pBuffer);
			free(pResultRecv);
			return;
		}
		
		printf("calling asrHandler.storeData\n");
		fflush(stdout);
		if(asrHandler.storeData(pBuffer,iNextWavSize) == false){
			free(pBuffer);
			free(pResultRecv);
			return;
		}
		free(pBuffer);
		
		//----------------------------------------------------------------
		int iSize = 0;
		char* pDataRecv = pResultRecv + iResultLen;
		//当结果字符串过长时，这里会溢出
		printf("calling asrHandler.sendData\n");
		fflush(stdout);
		if(asrHandler.sendData(pDataRecv,&iSize) == false){
			free(pResultRecv);
			return;
		}
		if(iSize > 0){
			iResultLen += iSize;
			pDataRecv[iSize] = '\0';
			printf("%s\n",pResultRecv);
			fflush(stdout);
			if(server.sendData(pResultRecv,iResultLen) == false){
				free(pResultRecv);
				return;
			}
		}
	}
	//----------------------------------------------------------------
	//finish getting result from ifly and return to client
	char* pDataRecv = pResultRecv + iResultLen;
	int iSize = 0;
	if(asrHandler.finishSend(pDataRecv,&iSize) == false){
		free(pResultRecv);
		return;
	}
	if(iSize > 0){
		iResultLen += iSize;
		pDataRecv[iSize] = '\0';
		printf("%s\n",pResultRecv);
		if(server.sendData(pResultRecv,iResultLen) == false){
			free(pResultRecv);
			return;
		}
	}

	const char* pFinish = "RESULT:DONE";
	memcpy(pResultRecv + iResultLen,pFinish,11+1);//including '\0'
	iResultLen += 11;
	printf("%s\n",pResultRecv);
	server.sendData(pResultRecv,iResultLen);
	
	//----------------------------------------------------------------
	//destruct function will end this client link
}
//---------------------------------------------------------------------------------------------------------------
int main() {
	if(Server::beginServer() == false){
		exit(1);
	}
	if(AsrHandler::login() == false){
		exit(1);
	}

	while (1)
	{
		//-----getting conf data-----
		printf("waitting for client...\n");
		int iClntSock;
		if(Server::acceptClient(&iClntSock) == false){
			printf("error! accept failed!\n");
			continue;
		}
		std::thread* t = new std::thread(handleLink,iClntSock);
	}
	AsrHandler::logout();
	Server::closeServer();
	return 0;
}
