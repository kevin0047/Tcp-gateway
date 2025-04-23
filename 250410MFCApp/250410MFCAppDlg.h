#pragma once
#include <afxsock.h>
#include <vector>
#include <afxcmn.h>

// CModbusTcpSocket 클래스: 소켓 통신 처리
class CModbusTcpSocket : public CAsyncSocket
{
public:
    CModbusTcpSocket();
    virtual ~CModbusTcpSocket();

    void SetParent(CDialog* pParent) { m_pParent = pParent; }
    void SetSocketID(int nID) { m_nSocketID = nID; }
    int GetSocketID() const { return m_nSocketID; }
    bool IsConnected() const { return m_bConnected; }
    // 리슨 모드 추가
    void SetListenMode(bool bListen) { m_bListenMode = bListen; }
    bool IsListenMode() const { return m_bListenMode; }

    // 오버라이드된 비동기 소켓 함수
    virtual void OnAccept(int nErrorCode);
    virtual void OnConnect(int nErrorCode);
    virtual void OnReceive(int nErrorCode);
    virtual void OnClose(int nErrorCode);
    virtual void OnSend(int nErrorCode);
    bool m_bConnected;   // 연결 상태
    // 데이터 송신 함수
    BOOL SendData(const BYTE* pData, int nLength);
    std::vector<BYTE> m_RecvBuffer;  // 수신 버퍼

private:
    CDialog* m_pParent;  // 부모 대화상자
    int m_nSocketID;     // 소켓 식별자
    bool m_bListenMode;  // 리슨 모드 여부
};

// 인디케이터-PLC 연결 쌍 정보를 저장하는 구조체
struct ConnectionPair {
    CString indicatorIP;
    int indicatorPort;
    int unitID;
    int plcPort;  // 자동 할당된 PLC 서버 포트
    CModbusTcpSocket indicatorSocket;  // 인디케이터와 통신하는 클라이언트 소켓
    CModbusTcpSocket serverSocket;     // 서버 모드로 대기하는 리스닝 소켓
    CModbusTcpSocket clientSocket;     // Accept 후 실제 통신에 사용하는 소켓
    bool indicatorConnected;
    bool plcConnected;
};

// 250410MFCAppDlg 대화 상자
class C250410MFCAppDlg : public CDialogEx
{
    // 생성
public:
    C250410MFCAppDlg(CWnd* pParent = nullptr);
    virtual ~C250410MFCAppDlg();

    // 대화 상자 데이터
#ifdef AFX_DESIGN_TIME
    enum { IDD = IDD_MY250410MFCAPP_DIALOG };
#endif

protected:
    virtual void DoDataExchange(CDataExchange* pDX);
    virtual BOOL OnInitDialog();
    afx_msg void OnPaint();
    afx_msg HCURSOR OnQueryDragIcon();

    HICON m_hIcon;
    DECLARE_MESSAGE_MAP()

public:
    // 소켓 ID 정의
    enum {
        SOCKET_INDICATOR = 0,
        SOCKET_SERVER = 1,
        SOCKET_CLIENT = 2
    };

    // 소켓 콜백 함수 (CModbusTcpSocket에서 호출)
    afx_msg LRESULT OnSocketConnect(WPARAM wParam, LPARAM lParam);
    afx_msg LRESULT OnSocketReceive(WPARAM wParam, LPARAM lParam);
    afx_msg LRESULT OnSocketClose(WPARAM wParam, LPARAM lParam);
    afx_msg LRESULT OnSocketAccept(WPARAM wParam, LPARAM lParam);

    // 컨트롤 변수
    CListBox m_connectionList;  // 연결 목록 표시용
    CListBox m_logList;
    CButton m_btnConnectAll;    // 모든 연결 시작/중단 버튼
    CButton m_btnClearLog;

    // 컨트롤 핸들러
    afx_msg void OnBnClickedBtnConnectAll();
    afx_msg void OnBnClickedBtnClearLog();

    // 변경 후
    static const int MAX_CONNECTIONS = 10;  // 최대 연결 개수
    ConnectionPair m_connections[MAX_CONNECTIONS];
    int m_connectionCount;  // 실제 사용 중인 연결 개수

    // 연결 관리 기능
    BOOL LoadConnectionsFromFile(LPCTSTR lpszFilePath);
    void InitializeConnections();
    int FindConnectionBySocketID(int nSocketID);
    void ConnectIndicator(int index);
    void StartPLCServer(int index);

    // UI 변경 관련
    void UpdateConnectionList();
    void UpdateStatusDisplay();

    // 통신 관련 함수
    void IndicatorToPLC(int index, const BYTE* pData, int nLength);
    void PLCToIndicator(int index, const BYTE* pData, int nLength);

    // 로그 함수
    void AddLog(LPCTSTR pszPrefix, const BYTE* pData, int nLength);
    void AddLog(LPCTSTR pszLog);

    // Modbus 패킷 분석 함수
    CString AnalyzeModbusPacket(const BYTE* pData, int nLength);
    // 포트 번호 찾기 함수
    int FindAvailablePort(int nStartPort, int nEndPort);
};