#pragma once
#define MAX_FILE_TEAM 128*1024
#include <afxmt.h>

typedef struct _DataPak
{
	int dataLen;	// ���ݳ��ȣ���λΪByte
	BYTE* data;		// ��������

}DataPak;
// ���ļ���
// 1. ѡ��·���󣬻��Զ�����ѡ·���´�����ʱ���йص��ļ����洢���ݣ�
// 2. �������õ����ļ������洢�����������ļ��Ĵ�С�ﵽ���洢��ʱ������һ�����ļ����ڴ洢���ݣ�
// 3. ��ǰд���ļ��洢��δ�ﵽ���洢��ʱ��Ҳ����ָ�����´������ļ����ڴ洢���ݣ�
class FileSave
{
public:
	FileSave();	// ���캯��
	virtual ~FileSave();
public:
	// �ļ����
	int SetFilePath(CString str);	// ���ݴ�����ַ��������ļ�·�� 
	int SelectFilePath();			// ��windows��Դ������ѡ���ļ�·��
	int Write(BYTE* data,int len);	// д���ݵ��ļ�
	int SetMaxFileSize(int size);	// ָ���ļ��������
private:
	int Save(DataPak *pDataPak);	// ���ļ�
	void CloseFileSave();			// �رմ��ļ�

	// �߳���ؽӿ�
public:
	void SetThreadExit(bool ex);
	bool GetThreadRunStatus();
	int GetDataFromTeam(DataPak *datapack);
	int TreadSave(DataPak *pDataPak);	// �̴߳��ļ�
private:
	void InitDataSaveToFileThread();	// ��ʼ�����ļ��߳�
	void StartDataSaveToFileThread();	// �������ļ��߳�
	void StopDataSaveToFileThread();	// �رմ��ļ��߳�

	// �������
	void InitSaveDataTeam();	// ���г�ʼ��
	int SaveDataTeamFull();				// �ж϶����Ƿ���
	int InsertSaveDataToTeam(BYTE* data,int len, BOOL bWait);		// �����ݲ�����ļ�����
	int PickOutSaveDataFromTeam(DataPak *datapack);					// �����ݴӴ��ļ�����ȡ��
	void DeleteSaveDataTeam();										// �ͷŴ��ļ����ж�̬������ڴ�
private:
	CFile m_file;		// �ļ�
	CString m_filepath;	// �ļ�·�����ļ�������
	CString m_filename;	// ��ǰд���ļ��������ļ�·����
	int m_MaxFileSize;	// �����ļ����洢������λΪByte��
	int m_WriteFileSize;// ��ǰд�ļ���С����λΪByte��

	bool m_FileSaveOpen;			// �ļ��򿪱�־���жϵ�ǰ�Ƿ������ڴ洢���ļ���
	bool m_FileSaveThreadRun;		// ���ļ��̴߳򿪱�־
	bool m_FileSaveThreadExit;		// ���ļ��߳��˳���־
	
	// �߳����
	DataPak* m_SaveDataTeamIn;
	DataPak* m_SaveDataTeamOut;
	DataPak m_SaveDataTeam[MAX_FILE_TEAM];
	CCriticalSection m_InsSaveDataCriSec;
	CCriticalSection m_PopSaveDataCriSec;
};