#include "stdafx.h"
#include "FileSave.h"

//------------------------------------------------------------------------------
// ���ļ��߳�
UINT FileSaveThread(LPVOID pParam)
{
	FileSave *filesave = (FileSave*)pParam;

	filesave->SetThreadExit(false);	                                        // �߳��˳���־��FALSE
	// g_WriteFileLength = 0;
	while(filesave->GetThreadRunStatus())  
	{
		DataPak pDataPak;

		int SaveData = filesave->GetDataFromTeam(&pDataPak);	// ������ʱ����1

		if(SaveData>0)					
		{
			filesave->TreadSave(&pDataPak);
		}
		else
		{
			// ����Ϊ��ʱ�ȴ�
			Sleep(10);
		}
	}
	filesave->SetThreadExit(true);	 		// �߳��˳���־��TRUE

	return 0;
}

//------------------------------------------------------------------------------
// ��ȡexe�ļ�·������
CString GetModuleFilePath_class()
{
	TCHAR ModuleFileName[MAX_PATH];
	::GetModuleFileName(AfxGetApp()->m_hInstance,ModuleFileName,MAX_PATH);
	TCHAR drive[_MAX_DRIVE];
	TCHAR dir[_MAX_DIR];
	TCHAR fname[_MAX_FNAME];
	TCHAR ext[_MAX_EXT];
	//�ָ�����·����
	_tsplitpath_s(ModuleFileName,drive,dir,fname,ext);
	CString FilePath;
	FilePath.Format(_T("%s\\%s"), drive , dir);
	return FilePath;
}


// ѡ���ļ�·��
int CALLBACK BrowseCallbackProc(HWND hwnd, UINT uMsg, LPARAM lParam, LPARAM lpData)   
{   
	switch(uMsg)
	{   
	case BFFM_INITIALIZED: 
		{		
			USES_CONVERSION;
			char * pFileName = T2A(GetModuleFilePath_class()); 
			// strcpy(oldFilePath,GetModuleFilePath_class());//��ʼȱʡ�ļ�����
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
// ���ݴ��ļ����г�ʼ��
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
// �ж϶����Ƿ���
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
// �������ݴ��ļ�����
// ������ʱ bWaitΪTrue �ȴ���bWaitΪfalse ���ȴ����ش���
int FileSave::InsertSaveDataToTeam(BYTE* data,int len, BOOL bWait)
{
	m_InsSaveDataCriSec.Lock();										// ʹ���ٽ�������

	if(bWait)// ������ʱ�ȴ�
	{
		m_PopSaveDataCriSec.Lock();

		while(SaveDataTeamFull() && m_FileSaveThreadRun)				// ��T�߳���Ҫֹͣʱ���˴�Ӧ��������д���ݣ�
		{
			if(m_SaveDataTeamOut == (m_SaveDataTeam + MAX_FILE_TEAM - 1))
				m_SaveDataTeamOut = m_SaveDataTeam;
			else
				m_SaveDataTeamOut++;
		}

		m_PopSaveDataCriSec.Unlock();
	}
	else// ������ʱ���ش���
	{
		if(SaveDataTeamFull())
		{
			m_InsSaveDataCriSec.Unlock();								// ����
			return -1;
		}
	}

	// �����
	m_SaveDataTeamIn->dataLen = len;
	m_SaveDataTeamIn->data = new BYTE[len];
	memcpy(m_SaveDataTeamIn->data, data, len);
	
	if(m_SaveDataTeamIn == (m_SaveDataTeam + MAX_FILE_TEAM - 1))
		m_SaveDataTeamIn = m_SaveDataTeam;
	else
		m_SaveDataTeamIn++;

	m_InsSaveDataCriSec.Unlock();										// ����

	return 0;
}

//------------------------------------------------------------------------------
// ������ݶ���
// ������ʱ����1
int FileSave::PickOutSaveDataFromTeam(DataPak *datapack)
{	
	
	if(m_SaveDataTeamIn != m_SaveDataTeamOut)
	{
		m_PopSaveDataCriSec.Lock();
		// ������
		datapack->dataLen = m_SaveDataTeamOut->dataLen;
		datapack->data = new BYTE[datapack->dataLen];
		memcpy(datapack->data, m_SaveDataTeamOut->data, datapack->dataLen);
		m_SaveDataTeamOut->dataLen = 0;
		delete m_SaveDataTeamOut->data;
		// �´γ�����λ������
		if(m_SaveDataTeamOut == (m_SaveDataTeam + MAX_FILE_TEAM - 1))
			m_SaveDataTeamOut = m_SaveDataTeam;
		else
			m_SaveDataTeamOut++;
		m_PopSaveDataCriSec.Unlock();
		return 1;
	}
	return 0;
}

// �ͷŴ��ļ����ж�̬������ڴ�
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
// ���Ա��������
FileSave::FileSave()
{
	m_FileSaveOpen = false;
	m_FileSaveThreadRun = false;
	m_WriteFileSize = 0;
	m_MaxFileSize = 1024*1024*1024; // Ĭ������1GB����
	InitDataSaveToFileThread();
	InitSaveDataTeam();
	m_filepath = _T("");
	StartDataSaveToFileThread();	// �������ļ��߳�
}

FileSave::~FileSave()
{
	if (m_FileSaveOpen)
	{
		m_file.Close();
		m_FileSaveOpen = false;
	}
	StopDataSaveToFileThread();
	// ��һ����ն��еĺ���
	DeleteSaveDataTeam();
}

// ͨ���ַ��������ļ�·��
int FileSave::SetFilePath(CString str)
{
	m_filepath = str;
	// �����жϴ����·���Ƿ���Ч
	return 0;
}

// ͨ���Ի���ѡ���ļ�·��
int FileSave::SelectFilePath()
{
	if(0)
	{
		AfxMessageBox(_T("���������·����"));
	}
	else
	{
		BROWSEINFO bi;
		TCHAR Buffer[1024];
		//��ʼ����ڲ���bi��ʼ
		bi.hwndOwner = NULL;
		bi.pidlRoot = NULL;
		bi.pszDisplayName = Buffer;//�˲�����ΪNULL������ʾ�Ի���
		bi.lpszTitle = _T("ѡ���ļ���");
		bi.ulFlags = 0;
		bi.lpfn = BrowseCallbackProc;
		//bi.iImage=IDI_ICON2;
		//��ʼ����ڲ���bi����
		LPITEMIDLIST pIDList = SHBrowseForFolder(&bi);//������ʾѡ��Ի���
		if(pIDList)
		{
			SHGetPathFromIDList(pIDList, Buffer);	//ȡ���ļ���·����Buffer��

			CString DataDir = Buffer;	//��·��������һ��CString������

			// �ж���ѡ·���Ƿ���Ч
			if (DataDir != _T(""))
			{
				m_filepath = DataDir;
			}
			else
			{
				CString csErr;
				csErr.Format(_T("��ѡ·����Ч"));
				AfxMessageBox(csErr);	            // ������ʾ�豸��ʧ��
			}
		}

		LPMALLOC lpMalloc;
		if(FAILED(SHGetMalloc(&lpMalloc))) return -1;
		//�ͷ��ڴ�
		lpMalloc->Free(pIDList);
		lpMalloc->Release();
	}
	return 0;
}

// д���ݵ��ļ�
int FileSave::Write(BYTE* data,int len)
{
	// Ĭ�ϲ��������ٶȴ��ڴ��ļ��ٶ�ʱ�ȴ�
	return InsertSaveDataToTeam(data, len, 1);
}

// ָ���ļ��������
int FileSave::SetMaxFileSize(int size)
{
	if (!m_FileSaveOpen)
	{
		m_MaxFileSize = size;
		return 0;
	}else{
		CString csErr;
		csErr.Format(_T("�ļ��Ѵ򿪣������޸��ļ����������"));
		AfxMessageBox(csErr);	            // ������ʾ�豸��ʧ��
		return -1;
	}
}

// ���ļ�
int FileSave::Save(DataPak *pDataPak)
{
	// ����Save����Ĭ�Ͽ������ļ��̲߳���ʼ���ļ�
	if (m_WriteFileSize + pDataPak->dataLen > m_MaxFileSize)
	{
		// �����д�ļ�����������ļ������½��ļ����ڴ洢
		m_FileSaveOpen = false;
	}
	if (!m_FileSaveOpen)
	{
		// �ļ�δ��ʱ�����������ļ�
		m_WriteFileSize = 0;
		// ��ȡϵͳʱ��
		SYSTEMTIME st;
		GetLocalTime(&st);

		m_filename.Format(_T("%s\\Chdata_%04d%02d%02d-%02d%02d%02d.dat"),
			m_filepath, 
			st.wYear, st.wMonth, st.wDay, st.wHour, st.wMinute, st.wSecond);  // �ļ�������ʵ������޸�
		m_FileSaveOpen = true;

		// �����ļ�
		if (!m_file.Open(m_filename, CFile::modeCreate|CFile::modeReadWrite|CFile::modeNoTruncate))
		{
			AfxMessageBox(_T("�ļ�����ʧ�ܣ�"));
			return 0;
		}
		
	}

	// ���ļ�
	m_file.Write(pDataPak->data, pDataPak->dataLen);
	delete pDataPak->data;
	return 1;
}

// �رմ��ļ�
void FileSave::CloseFileSave()
{
	if(m_FileSaveOpen)
	{
		m_file.Close();
		m_FileSaveOpen = false;
	}
}

// �����߳��˳���־
void FileSave::SetThreadExit(bool ex)
{
	m_FileSaveThreadExit = ex;
}

// ��ȡ�߳����б�־
bool FileSave::GetThreadRunStatus()
{
	return m_FileSaveThreadRun;
}

// �Ӷ���ȡ������
int FileSave::GetDataFromTeam(DataPak *datapack)
{
	return PickOutSaveDataFromTeam(datapack);
}

int FileSave::TreadSave(DataPak *pDataPak)
{
	return Save(pDataPak);
}

// ��ʼ�����ļ��̺߳���
void FileSave::InitDataSaveToFileThread()
{
	m_FileSaveThreadRun = FALSE;
	m_FileSaveThreadExit = TRUE;
}

// �������ļ��̺߳���
void FileSave::StartDataSaveToFileThread()
{
	m_FileSaveThreadRun = TRUE;

	// �ڶ�������Ϊ�����̵߳Ĳ���������ΪLPVOID
	AfxBeginThread(FileSaveThread, (LPVOID)this, THREAD_PRIORITY_NORMAL, 0, 0, NULL);
}

// ֹͣ���ļ��̺߳���
void FileSave::StopDataSaveToFileThread()
{
	m_FileSaveThreadRun = FALSE;

	// �ȴ��������߳��˳�
	while(!m_FileSaveThreadExit)
		Sleep(10);
}