#pragma once
#define MAX_FILE_TEAM 128*1024
#include <afxmt.h>

typedef struct _DataPak
{
	int dataLen;	// 数据长度，单位为Byte
	BYTE* data;		// 数据内容

}DataPak;
// 存文件类
// 1. 选择路径后，会自动在所选路径下创建和时间有关的文件并存储数据；
// 2. 可以设置单个文件的最大存储量，当所存文件的大小达到最大存储量时，创建一个新文件用于存储数据；
// 3. 当前写入文件存储量未达到最大存储量时，也可以指定重新创建新文件用于存储数据；
class FileSave
{
public:
	FileSave();	// 构造函数
	virtual ~FileSave();
public:
	// 文件相关
	int SetFilePath(CString str);	// 根据传入的字符串设置文件路径 
	int SelectFilePath();			// 从windows资源管理器选择文件路径
	int Write(BYTE* data,int len);	// 写数据到文件
	int SetMaxFileSize(int size);	// 指定文件最大容量
private:
	int Save(DataPak *pDataPak);	// 存文件
	void CloseFileSave();			// 关闭存文件

	// 线程相关接口
public:
	void SetThreadExit(bool ex);
	bool GetThreadRunStatus();
	int GetDataFromTeam(DataPak *datapack);
	int TreadSave(DataPak *pDataPak);	// 线程存文件
private:
	void InitDataSaveToFileThread();	// 初始化存文件线程
	void StartDataSaveToFileThread();	// 启动存文件线程
	void StopDataSaveToFileThread();	// 关闭存文件线程

	// 队列相关
	void InitSaveDataTeam();	// 队列初始化
	int SaveDataTeamFull();				// 判断队列是否满
	int InsertSaveDataToTeam(BYTE* data,int len, BOOL bWait);		// 将数据插入存文件队列
	int PickOutSaveDataFromTeam(DataPak *datapack);					// 将数据从存文件队列取出
	void DeleteSaveDataTeam();										// 释放存文件队列动态申请的内存
private:
	CFile m_file;		// 文件
	CString m_filepath;	// 文件路径（文件夹名）
	CString m_filename;	// 当前写入文件（完整文件路径）
	int m_MaxFileSize;	// 单个文件最大存储量（单位为Byte）
	int m_WriteFileSize;// 当前写文件大小（单位为Byte）

	bool m_FileSaveOpen;			// 文件打开标志，判断当前是否有用于存储的文件打开
	bool m_FileSaveThreadRun;		// 存文件线程打开标志
	bool m_FileSaveThreadExit;		// 存文件线程退出标志
	
	// 线程相关
	DataPak* m_SaveDataTeamIn;
	DataPak* m_SaveDataTeamOut;
	DataPak m_SaveDataTeam[MAX_FILE_TEAM];
	CCriticalSection m_InsSaveDataCriSec;
	CCriticalSection m_PopSaveDataCriSec;
};