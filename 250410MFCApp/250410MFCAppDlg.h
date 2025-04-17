#pragma once
#include <afxsock.h>
#include <vector>
#include <afxcmn.h>

// CModbusTcpSocket 클래스: 소켓 통신 처리
class CModbusTcpSocket : public CAsyncSocket
{
public:


    // Accept 이벤트 처리 추가
    virtual void OnAccept(int nErrorCode);

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
    CIPAddressCtrl m_ipIndicator;
    CIPAddressCtrl m_ipPLC;
    CEdit m_portIndicator;
    CEdit m_portPLC;
    CListBox m_logList;
    CButton m_btnConnectIndicator;
    CButton m_btnConnectPLC;
    CButton m_btnClearLog;
    CStatic m_statusIndicator;
    CStatic m_statusPLC;

    // 컨트롤 핸들러
    afx_msg void OnBnClickedBtnConnectIndicator();
    afx_msg void OnBnClickedBtnConnectPlc();
    afx_msg void OnBnClickedBtnClearLog();

    // 통신 관련 변수 및 함수
    CModbusTcpSocket m_indicatorSocket;  // 인디케이터와 통신하는 클라이언트 소켓
    CModbusTcpSocket m_serverSocket;     // 서버 모드로 대기하는 리스닝 소켓
    CModbusTcpSocket m_clientSocket;     // Accept 후 실제 통신에 사용하는 소켓
    bool m_bIndicatorConnected;
    bool m_bPlcConnected;                // 서버 모드 동작 여부로 의미 변경
    // 중계 함수
    void IndicatorToPLC(const BYTE* pData, int nLength);
    void PLCToIndicator(const BYTE* pData, int nLength);

    // 로그 함수
    void AddLog(LPCTSTR pszPrefix, const BYTE* pData, int nLength);
    void AddLog(LPCTSTR pszLog);

    // Modbus 패킷 분석 함수
    CString AnalyzeModbusPacket(const BYTE* pData, int nLength);
    //포트 번호 찾기 함수
    int FindAvailablePort(int nStartPort, int nEndPort);
};
