#include "stdafx.h"
#include "FileSave.h"

//------------------------------------------------------------------------------
// 存文件线程
UINT FileSaveThread(LPVOID pParam)
{
	FileSave *filesave = (FileSave*)pParam;

	filesave->SetThreadExit(false);	                                        // 线程退出标志置FALSE
	// g_WriteFileLength = 0;
	while(filesave->GetThreadRunStatus())  
	{
		DataPak pDataPak;

		int SaveData = filesave->GetDataFromTeam(&pDataPak);	// 有数据时返回1

		if(SaveData>0)					
		{
			filesave->TreadSave(&pDataPak);
		}
		else
		{
			// 队列为空时等待
			Sleep(10);
		}
	}
	filesave->SetThreadExit(true);	 		// 线程退出标志置TRUE

	return 0;
}

//------------------------------------------------------------------------------
// 获取exe文件路径函数
CString GetModuleFilePath_class()
{
	TCHAR ModuleFileName[MAX_PATH];
	::GetModuleFileName(AfxGetApp()->m_hInstance,ModuleFileName,MAX_PATH);
	TCHAR drive[_MAX_DRIVE];
	TCHAR dir[_MAX_DIR];
	TCHAR fname[_MAX_FNAME];
	TCHAR ext[_MAX_EXT];
	//分割完整路径名
	_tsplitpath_s(ModuleFileName,drive,dir,fname,ext);
	CString FilePath;
	FilePath.Format(_T("%s\\%s"), drive , dir);
	return FilePath;
}


// 选择文件路径
int CALLBACK BrowseCallbackProc(HWND hwnd, UINT uMsg, LPARAM lParam, LPARAM lpData)   
{   
	switch(uMsg)
	{   
	case BFFM_INITIALIZED: 
		{		
			USES_CONVERSION;
			char * pFileName = T2A(GetModuleFilePath_class()); 
			// strcpy(oldFilePath,GetModuleFilePath_class());//初始缺省文件夹名
			::SendMessage(hwnd,BFFM_SETSELECTION,TRUE,(LPARAM)&pFileName);   
		}

		break;   
	case BFFM_SELCHANGED:   
		{   
			TCHAR curr[MAX_PATH];   
			SHGetPathFromIDList((LPCITEMIDLIST)lParam, curr);   
			::SendMessage(hwnd, BFFM_SETSTATUSTEXT, 0, (LPARAM)curr);   
		}   
		break;   
	}   
	return 0;   
} 


//------------------------------------------------------------------------------

//------------------------------------------------------------------------------
// 数据存文件队列初始化
void FileSave::InitSaveDataTeam()
{
	m_SaveDataTeamIn = m_SaveDataTeamOut = m_SaveDataTeam;
	for (int i = 0; i < MAX_FILE_TEAM; i++)
	{
		m_SaveDataTeam[i].dataLen = 0;
		m_SaveDataTeam[i].data = NULL;
	}
}

//------------------------------------------------------------------------------
// 判断队列是否满
int FileSave::SaveDataTeamFull()
{
	int nItems;
	if(m_SaveDataTeamIn >= m_SaveDataTeamOut)
		nItems = m_SaveDataTeamIn - m_SaveDataTeamOut;
	else
		nItems = MAX_FILE_TEAM - (m_SaveDataTeamOut - m_SaveDataTeamIn);

	if(nItems >= (MAX_FILE_TEAM - 1))
		return 1;
	else
		return 0;
}


//------------------------------------------------------------------------------
// 插入数据存文件队列
// 队列满时 bWait为True 等待，bWait为false 不等待返回错误
int FileSave::InsertSaveDataToTeam(BYTE* data,int len, BOOL bWait)
{
	m_InsSaveDataCriSec.Lock();										// 使用临界区加锁

	if(bWait)// 队列满时等待
	{
		m_PopSaveDataCriSec.Lock();

		while(SaveDataTeamFull() && m_FileSaveThreadRun)				// 当T线程需要停止时，此处应放弃继续写数据；
		{
			if(m_SaveDataTeamOut == (m_SaveDataTeam + MAX_FILE_TEAM - 1))
				m_SaveDataTeamOut = m_SaveDataTeam;
			else
				m_SaveDataTeamOut++;
		}

		m_PopSaveDataCriSec.Unlock();
	}
	else// 队列满时返回错误
	{
		if(SaveDataTeamFull())
		{
			m_InsSaveDataCriSec.Unlock();								// 解锁
			return -1;
		}
	}

	// 入队列
	m_SaveDataTeamIn->dataLen = len;
	m_SaveDataTeamIn->data = new BYTE[len];
	memcpy(m_SaveDataTeamIn->data, data, len);
	
	if(m_SaveDataTeamIn == (m_SaveDataTeam + MAX_FILE_TEAM - 1))
		m_SaveDataTeamIn = m_SaveDataTeam;
	else
		m_SaveDataTeamIn++;

	m_InsSaveDataCriSec.Unlock();										// 解锁

	return 0;
}

//------------------------------------------------------------------------------
// 捡出数据队列
// 有数据时返回1
int FileSave::PickOutSaveDataFromTeam(DataPak *datapack)
{	
	
	if(m_SaveDataTeamIn != m_SaveDataTeamOut)
	{
		m_PopSaveDataCriSec.Lock();
		// 出队列
		datapack->dataLen = m_SaveDataTeamOut->dataLen;
		datapack->data = new BYTE[datapack->dataLen];
		memcpy(datapack->data, m_SaveDataTeamOut->data, datapack->dataLen);
		m_SaveDataTeamOut->dataLen = 0;
		delete m_SaveDataTeamOut->data;
		// 下次出数据位置增加
		if(m_SaveDataTeamOut == (m_SaveDataTeam + MAX_FILE_TEAM - 1))
			m_SaveDataTeamOut = m_SaveDataTeam;
		else
			m_SaveDataTeamOut++;
		m_PopSaveDataCriSec.Unlock();
		return 1;
	}
	return 0;
}

// 释放存文件队列动态申请的内存
void FileSave::DeleteSaveDataTeam()
{
	for (int i = 0; i < MAX_FILE_TEAM; i++)
	{
		if(m_SaveDataTeam[i].dataLen != 0)
		{
			delete m_SaveDataTeam[i].data;
		}
	}
}

//------------------------------------------------------------------------------
// 类成员函数定义
FileSave::FileSave()
{
	m_FileSaveOpen = false;
	m_FileSaveThreadRun = false;
	m_WriteFileSize = 0;
	m_MaxFileSize = 1024*1024*1024; // 默认最多存1GB数据
	InitDataSaveToFileThread();
	InitSaveDataTeam();
	m_filepath = _T("");
	StartDataSaveToFileThread();	// 开启存文件线程
}

FileSave::~FileSave()
{
	if (m_FileSaveOpen)
	{
		m_file.Close();
		m_FileSaveOpen = false;
	}
	StopDataSaveToFileThread();
	// 加一个清空队列的函数
	DeleteSaveDataTeam();
}

// 通过字符串设置文件路径
int FileSave::SetFilePath(CString str)
{
	m_filepath = str;
	// 后续判断传入的路径是否有效
	return 0;
}

// 通过对话框选择文件路径
int FileSave::SelectFilePath()
{
	if(0)
	{
		AfxMessageBox(_T("不允许更改路径！"));
	}
	else
	{
		BROWSEINFO bi;
		TCHAR Buffer[1024];
		//初始化入口参数bi开始
		bi.hwndOwner = NULL;
		bi.pidlRoot = NULL;
		bi.pszDisplayName = Buffer;//此参数如为NULL则不能显示对话框
		bi.lpszTitle = _T("选择文件夹");
		bi.ulFlags = 0;
		bi.lpfn = BrowseCallbackProc;
		//bi.iImage=IDI_ICON2;
		//初始化入口参数bi结束
		LPITEMIDLIST pIDList = SHBrowseForFolder(&bi);//调用显示选择对话框
		if(pIDList)
		{
			SHGetPathFromIDList(pIDList, Buffer);	//取得文件夹路径到Buffer里

			CString DataDir = Buffer;	//将路径保存在一个CString对象里

			// 判断所选路径是否有效
			if (DataDir != _T(""))
			{
				m_filepath = DataDir;
			}
			else
			{
				CString csErr;
				csErr.Format(_T("所选路径无效"));
				AfxMessageBox(csErr);	            // 弹窗提示设备打开失败
			}
		}

		LPMALLOC lpMalloc;
		if(FAILED(SHGetMalloc(&lpMalloc))) return -1;
		//释放内存
		lpMalloc->Free(pIDList);
		lpMalloc->Release();
	}
	return 0;
}

// 写数据到文件
int FileSave::Write(BYTE* data,int len)
{
	// 默认插入数据速度大于存文件速度时等待
	return InsertSaveDataToTeam(data, len, 1);
}

// 指定文件最大容量
int FileSave::SetMaxFileSize(int size)
{
	if (!m_FileSaveOpen)
	{
		m_MaxFileSize = size;
		return 0;
	}else{
		CString csErr;
		csErr.Format(_T("文件已打开，不可修改文件最大容量！"));
		AfxMessageBox(csErr);	            // 弹窗提示设备打开失败
		return -1;
	}
}

// 存文件
int FileSave::Save(DataPak *pDataPak)
{
	// 调用Save函数默认开启存文件线程并开始存文件
	if (m_WriteFileSize + pDataPak->dataLen > m_MaxFileSize)
	{
		// 如果所写文件量大于最大文件量，新建文件用于存储
		m_FileSaveOpen = false;
	}
	if (!m_FileSaveOpen)
	{
		// 文件未打开时，创建并打开文件
		m_WriteFileSize = 0;
		// 获取系统时间
		SYSTEMTIME st;
		GetLocalTime(&st);

		m_filename.Format(_T("%s\\Chdata_%04d%02d%02d-%02d%02d%02d.dat"),
			m_filepath, 
			st.wYear, st.wMonth, st.wDay, st.wHour, st.wMinute, st.wSecond);  // 文件名根据实际情况修改
		m_FileSaveOpen = true;

		// 创建文件
		if (!m_file.Open(m_filename, CFile::modeCreate|CFile::modeReadWrite|CFile::modeNoTruncate))
		{
			AfxMessageBox(_T("文件创建失败！"));
			return 0;
		}
		
	}

	// 存文件
	m_file.Write(pDataPak->data, pDataPak->dataLen);
	delete pDataPak->data;
	return 1;
}

// 关闭存文件
void FileSave::CloseFileSave()
{
	if(m_FileSaveOpen)
	{
		m_file.Close();
		m_FileSaveOpen = false;
	}
}

// 设置线程退出标志
void FileSave::SetThreadExit(bool ex)
{
	m_FileSaveThreadExit = ex;
}

// 获取线程运行标志
bool FileSave::GetThreadRunStatus()
{
	return m_FileSaveThreadRun;
}

// 从队列取出数据
int FileSave::GetDataFromTeam(DataPak *datapack)
{
	return PickOutSaveDataFromTeam(datapack);
}

int FileSave::TreadSave(DataPak *pDataPak)
{
	return Save(pDataPak);
}

// 初始化存文件线程函数
void FileSave::InitDataSaveToFileThread()
{
	m_FileSaveThreadRun = FALSE;
	m_FileSaveThreadExit = TRUE;
}

// 启动存文件线程函数
void FileSave::StartDataSaveToFileThread()
{
	m_FileSaveThreadRun = TRUE;

	// 第二个参数为传入线程的参数，类型为LPVOID
	AfxBeginThread(FileSaveThread, (LPVOID)this, THREAD_PRIORITY_NORMAL, 0, 0, NULL);
}

// 停止存文件线程函数
void FileSave::StopDataSaveToFileThread()
{
	m_FileSaveThreadRun = FALSE;

	// 等待所有主线程退出
	while(!m_FileSaveThreadExit)
		Sleep(10);
}