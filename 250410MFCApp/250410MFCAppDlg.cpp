#include "pch.h"
#include "framework.h"
#include "250410MFCApp.h"
#include "250410MFCAppDlg.h"
#include "afxdialogex.h"

#ifdef _DEBUG
#define new DEBUG_NEW
#endif

// 사용자 정의 메시지
#define WM_SOCKET_CONNECT (WM_USER + 1)
#define WM_SOCKET_RECEIVE (WM_USER + 2)
#define WM_SOCKET_CLOSE   (WM_USER + 3)
#define WM_SOCKET_ACCEPT   (WM_USER + 4)
// CModbusTcpSocket 클래스 구현
CModbusTcpSocket::CModbusTcpSocket()
    : m_pParent(NULL), m_nSocketID(0), m_bConnected(false), m_bListenMode(false)
{
}

CModbusTcpSocket::~CModbusTcpSocket()
{
    if (m_hSocket != INVALID_SOCKET)
        Close();
}

void CModbusTcpSocket::OnAccept(int nErrorCode)
{
    if (nErrorCode == 0)
    {
        m_pParent->PostMessage(WM_SOCKET_ACCEPT, m_nSocketID, 0);
    }
    CAsyncSocket::OnAccept(nErrorCode);
}
void CModbusTcpSocket::OnConnect(int nErrorCode)
{
    m_bConnected = (nErrorCode == 0);
    m_pParent->PostMessage(WM_SOCKET_CONNECT, m_nSocketID, nErrorCode);
    CAsyncSocket::OnConnect(nErrorCode);
}

void CModbusTcpSocket::OnReceive(int nErrorCode)
{
    if (nErrorCode == 0)
    {
        BYTE buffer[4096];
        int nRead = Receive(buffer, sizeof(buffer));
        if (nRead > 0)
        {
            // 버퍼에 데이터 추가
            m_RecvBuffer.insert(m_RecvBuffer.end(), buffer, buffer + nRead);

            // 데이터 처리를 위해 부모에게 알림
            m_pParent->PostMessage(WM_SOCKET_RECEIVE, m_nSocketID, m_RecvBuffer.size());
        }
    }
    CAsyncSocket::OnReceive(nErrorCode);
}

void CModbusTcpSocket::OnClose(int nErrorCode)
{
    m_bConnected = false;
    m_pParent->PostMessage(WM_SOCKET_CLOSE, m_nSocketID, nErrorCode);
    CAsyncSocket::OnClose(nErrorCode);
}

void CModbusTcpSocket::OnSend(int nErrorCode)
{
    CAsyncSocket::OnSend(nErrorCode);
}

BOOL CModbusTcpSocket::SendData(const BYTE* pData, int nLength)
{
    if (!m_bConnected || m_hSocket == INVALID_SOCKET)
        return FALSE;

    int nSent = Send(pData, nLength);
    return (nSent == nLength);
}

// C250410MFCAppDlg 대화 상자

C250410MFCAppDlg::C250410MFCAppDlg(CWnd* pParent /*=nullptr*/)
    : CDialogEx(IDD_MY250410MFCAPP_DIALOG, pParent)
    , m_bIndicatorConnected(false)
    , m_bPlcConnected(false)
{
    m_hIcon = AfxGetApp()->LoadIcon(IDR_MAINFRAME);
}

C250410MFCAppDlg::~C250410MFCAppDlg()
{
}

void C250410MFCAppDlg::DoDataExchange(CDataExchange* pDX)
{
    CDialogEx::DoDataExchange(pDX);
    DDX_Control(pDX, IDC_IPADDRESS_INDICATOR, m_ipIndicator);
    DDX_Control(pDX, IDC_IPADDRESS_PLC, m_ipPLC);
    DDX_Control(pDX, IDC_EDIT_PORT_INDICATOR, m_portIndicator);
    DDX_Control(pDX, IDC_EDIT_PORT_PLC, m_portPLC);
    DDX_Control(pDX, IDC_LIST_LOG, m_logList);
    DDX_Control(pDX, IDC_BTN_CONNECT_INDICATOR, m_btnConnectIndicator);
    DDX_Control(pDX, IDC_BTN_CONNECT_PLC, m_btnConnectPLC);
    DDX_Control(pDX, IDC_BTN_CLEAR_LOG, m_btnClearLog);
    DDX_Control(pDX, IDC_STATIC_STATUS_INDICATOR, m_statusIndicator);
    DDX_Control(pDX, IDC_STATIC_STATUS_PLC, m_statusPLC);
}

BEGIN_MESSAGE_MAP(C250410MFCAppDlg, CDialogEx)
    ON_WM_PAINT()
    ON_WM_QUERYDRAGICON()
    ON_BN_CLICKED(IDC_BTN_CONNECT_INDICATOR, &C250410MFCAppDlg::OnBnClickedBtnConnectIndicator)
    ON_BN_CLICKED(IDC_BTN_CONNECT_PLC, &C250410MFCAppDlg::OnBnClickedBtnConnectPlc)
    ON_BN_CLICKED(IDC_BTN_CLEAR_LOG, &C250410MFCAppDlg::OnBnClickedBtnClearLog)
    ON_MESSAGE(WM_SOCKET_CONNECT, &C250410MFCAppDlg::OnSocketConnect)
    ON_MESSAGE(WM_SOCKET_RECEIVE, &C250410MFCAppDlg::OnSocketReceive)
    ON_MESSAGE(WM_SOCKET_CLOSE, &C250410MFCAppDlg::OnSocketClose)
    ON_MESSAGE(WM_SOCKET_ACCEPT, &C250410MFCAppDlg::OnSocketAccept)
END_MESSAGE_MAP()

// C250410MFCAppDlg 메시지 처리기

BOOL C250410MFCAppDlg::OnInitDialog()
{
    CDialogEx::OnInitDialog();

    // 이 대화 상자의 아이콘을 설정합니다.
    SetIcon(m_hIcon, TRUE);
    SetIcon(m_hIcon, FALSE);

    // 소켓 초기화
    AfxSocketInit();

    // 소켓 설정
    m_indicatorSocket.SetParent(this);
    m_indicatorSocket.SetSocketID(SOCKET_INDICATOR);

    m_serverSocket.SetParent(this);
    m_serverSocket.SetSocketID(SOCKET_SERVER);
    m_serverSocket.SetListenMode(true);

    m_clientSocket.SetParent(this);
    m_clientSocket.SetSocketID(SOCKET_CLIENT);
    // UI 변경 - PLC 연결 그룹을 서버 모드로 변경
    m_btnConnectPLC.SetWindowText(_T("서버 시작"));
    // 기본 IP 및 포트 설정
    m_ipIndicator.SetAddress(127, 0, 0, 1);
    m_ipPLC.SetAddress(127, 0, 0, 1);
    m_portIndicator.SetWindowText(_T("5020"));  // Modbus-TCP 기본 포트
    m_portPLC.SetWindowText(_T("5030"));

    // 상태 표시 초기화
    m_statusIndicator.SetWindowText(_T("연결 안됨"));
    m_statusPLC.SetWindowText(_T("연결 안됨"));

    // 로그 초기화
    AddLog(_T("ModBus-TCP 릴레이 프로그램 시작"));
    AddLog(_T("인디케이터와 PLC 사이의 통신을 중계합니다."));

    return TRUE;
}

void C250410MFCAppDlg::OnPaint()
{
    if (IsIconic())
    {
        CPaintDC dc(this);

        SendMessage(WM_ICONERASEBKGND, reinterpret_cast<WPARAM>(dc.GetSafeHdc()), 0);

        int cxIcon = GetSystemMetrics(SM_CXICON);
        int cyIcon = GetSystemMetrics(SM_CYICON);
        CRect rect;
        GetClientRect(&rect);
        int x = (rect.Width() - cxIcon + 1) / 2;
        int y = (rect.Height() - cyIcon + 1) / 2;

        dc.DrawIcon(x, y, m_hIcon);
    }
    else
    {
        CDialogEx::OnPaint();
    }
}

HCURSOR C250410MFCAppDlg::OnQueryDragIcon()
{
    return static_cast<HCURSOR>(m_hIcon);
}

void C250410MFCAppDlg::OnBnClickedBtnConnectIndicator()
{
    if (!m_bIndicatorConnected)
    {
        // 연결 시도
        DWORD dwAddress;
        m_ipIndicator.GetAddress(dwAddress);

        CString strPort;
        m_portIndicator.GetWindowText(strPort);
        int nPort = _ttoi(strPort);

        CString strIP;
        strIP.Format(_T("%d.%d.%d.%d"),
            (dwAddress >> 24) & 0xFF,
            (dwAddress >> 16) & 0xFF,
            (dwAddress >> 8) & 0xFF,
            dwAddress & 0xFF);

        // 소켓 생성 및 연결
        if (m_indicatorSocket.Create())
        {
            if (m_indicatorSocket.Connect(strIP, nPort))
            {
                AddLog(_T("인디케이터 연결 중..."));
            }
            else
            {
                int nError = GetLastError();
                if (nError != WSAEWOULDBLOCK)
                {
                    CString strError;
                    strError.Format(_T("인디케이터 연결 실패: 오류 코드 %d"), nError);
                    AddLog(strError);
                    m_indicatorSocket.Close();
                }
            }
        }
        else
        {
            AddLog(_T("인디케이터 소켓 생성 실패"));
        }
    }
    else
    {
        // 연결 해제
        m_indicatorSocket.Close();
        m_bIndicatorConnected = false;
        m_btnConnectIndicator.SetWindowText(_T("연결"));
        m_statusIndicator.SetWindowText(_T("연결 안됨"));
        AddLog(_T("인디케이터 연결 해제"));
    }
}

void C250410MFCAppDlg::OnBnClickedBtnConnectPlc()
{
    if (!m_bPlcConnected)
    {
        // 연결 시도 (서버 모드)
        DWORD dwAddress;
        m_ipPLC.GetAddress(dwAddress);

        CString strPort;
        m_portPLC.GetWindowText(strPort);
        int nPort = _ttoi(strPort);

        // 서버 소켓 생성 및 바인딩
        if (m_serverSocket.Create(nPort))
        {
            if (m_serverSocket.Listen())
            {
                m_bPlcConnected = true;
                m_btnConnectPLC.SetWindowText(_T("중지"));
                m_statusPLC.SetWindowText(_T("대기 중..."));

                CString strLog;
                strLog.Format(_T("PLC 서버 모드 시작: 포트 %d 대기 중"), nPort);
                AddLog(strLog);
            }
            else
            {
                int nError = GetLastError();
                CString strError;
                strError.Format(_T("서버 리슨 실패: 오류 코드 %d"), nError);
                AddLog(strError);
                m_serverSocket.Close();
            }
        }
        else
        {
            AddLog(_T("서버 소켓 생성 실패"));
        }
    }
    else
    {
        // 서버 중지
        m_serverSocket.Close();
        m_clientSocket.Close();
        m_bPlcConnected = false;
        m_btnConnectPLC.SetWindowText(_T("서버 시작"));
        m_statusPLC.SetWindowText(_T("중지됨"));
        AddLog(_T("PLC 서버 모드 중지"));
    }
}
// 클라이언트 접속 처리 함수 추가
LRESULT C250410MFCAppDlg::OnSocketAccept(WPARAM wParam, LPARAM lParam)
{
    int nSocketID = (int)wParam;

    if (nSocketID == SOCKET_SERVER)
    {
        // 새로운 클라이언트 연결 수락
        if (m_clientSocket.m_hSocket != INVALID_SOCKET)
        {
            // 기존 연결이 있으면 닫기
            m_clientSocket.Close();
        }

        // 연결 수락
        SOCKADDR_IN clientAddr;
        int addrLen = sizeof(clientAddr);

        if (m_serverSocket.Accept(m_clientSocket, (SOCKADDR*)&clientAddr, &addrLen))
        {
            // 클라이언트 정보 추출
            CString strClientIP;
            strClientIP.Format(_T("%d.%d.%d.%d"),
                (int)(clientAddr.sin_addr.S_un.S_un_b.s_b1),
                (int)(clientAddr.sin_addr.S_un.S_un_b.s_b2),
                (int)(clientAddr.sin_addr.S_un.S_un_b.s_b3),
                (int)(clientAddr.sin_addr.S_un.S_un_b.s_b4));

            int nClientPort = ntohs(clientAddr.sin_port);

            CString strInfo;
            strInfo.Format(_T("클라이언트 접속: %s:%d"), strClientIP, nClientPort);
            AddLog(strInfo);

            // 클라이언트 소켓 설정
            m_clientSocket.SetSocketID(SOCKET_CLIENT);
            m_clientSocket.m_bConnected = true;
            m_statusPLC.SetWindowText(_T("클라이언트 연결됨"));
        }
        else
        {
            AddLog(_T("클라이언트 연결 수락 실패"));
        }
    }

    return 0;
}
void C250410MFCAppDlg::OnBnClickedBtnClearLog()
{
    m_logList.ResetContent();
    AddLog(_T("로그 지움"));
}

LRESULT C250410MFCAppDlg::OnSocketConnect(WPARAM wParam, LPARAM lParam)
{
    int nSocketID = (int)wParam;
    int nErrorCode = (int)lParam;

    if (nSocketID == SOCKET_INDICATOR)
    {
        if (nErrorCode == 0)
        {
            m_bIndicatorConnected = true;
            m_btnConnectIndicator.SetWindowText(_T("해제"));
            m_statusIndicator.SetWindowText(_T("연결됨"));
            AddLog(_T("인디케이터 연결 성공"));
        }
        else
        {
            CString strError;
            strError.Format(_T("인디케이터 연결 실패: 오류 코드 %d"), nErrorCode);
            AddLog(strError);
            m_indicatorSocket.Close();
        }
    }
    else if (nSocketID == SOCKET_CLIENT)
    {
        if (nErrorCode == 0)
        {
            m_bPlcConnected = true;
            m_btnConnectPLC.SetWindowText(_T("해제"));
            m_statusPLC.SetWindowText(_T("연결됨"));
            AddLog(_T("PLC 연결 성공"));
        }
        else
        {
            CString strError;
            strError.Format(_T("PLC 연결 실패: 오류 코드 %d"), nErrorCode);
            AddLog(strError);
            m_clientSocket.Close();
        }
    }

    return 0;
}

LRESULT C250410MFCAppDlg::OnSocketReceive(WPARAM wParam, LPARAM lParam)
{
    int nSocketID = (int)wParam;
    int nBytesAvailable = (int)lParam;

    if (nSocketID == SOCKET_INDICATOR)
    {
        // 인디케이터에서 데이터 수신
        std::vector<BYTE>& buffer = m_indicatorSocket.m_RecvBuffer;
        if (!buffer.empty())
        {
            // Modbus-TCP 패킷 분석 및 로그
            CString strAnalysis = AnalyzeModbusPacket(buffer.data(), (int)buffer.size());
            if (!strAnalysis.IsEmpty())
                AddLog(strAnalysis);

            // PLC로 데이터 전달
            IndicatorToPLC(buffer.data(), (int)buffer.size());

            // 버퍼 비우기
            buffer.clear();
        }
    }
    else if (nSocketID == SOCKET_CLIENT)  // SOCKET_PLC 대신 SOCKET_CLIENT 사용
    {
        // 클라이언트(PLC)에서 데이터 수신
        std::vector<BYTE>& buffer = m_clientSocket.m_RecvBuffer;  // m_plcSocket 대신 m_clientSocket 사용
        if (!buffer.empty())
        {
            // Modbus-TCP 패킷 분석 및 로그
            CString strAnalysis = AnalyzeModbusPacket(buffer.data(), (int)buffer.size());
            if (!strAnalysis.IsEmpty())
                AddLog(strAnalysis);

            // 인디케이터로 데이터 전달
            PLCToIndicator(buffer.data(), (int)buffer.size());

            // 버퍼 비우기
            buffer.clear();
        }
    }

    return 0;
}

LRESULT C250410MFCAppDlg::OnSocketClose(WPARAM wParam, LPARAM lParam)
{
    int nSocketID = (int)wParam;

    if (nSocketID == SOCKET_INDICATOR)
    {
        m_bIndicatorConnected = false;
        m_btnConnectIndicator.SetWindowText(_T("연결"));
        m_statusIndicator.SetWindowText(_T("연결 안됨"));
        AddLog(_T("인디케이터 연결 종료됨"));
    }
    else if (nSocketID == SOCKET_CLIENT)
    {
        m_bPlcConnected = false;
        m_btnConnectPLC.SetWindowText(_T("연결"));
        m_statusPLC.SetWindowText(_T("연결 안됨"));
        AddLog(_T("PLC 연결 종료됨"));
    }

    return 0;
}

void C250410MFCAppDlg::IndicatorToPLC(const BYTE* pData, int nLength)
{
    if (m_clientSocket.m_hSocket != INVALID_SOCKET && m_clientSocket.IsConnected())
    {
        AddLog(_T("인디케이터 → PLC: "), pData, nLength);
        BOOL bResult = m_clientSocket.SendData(pData, nLength);

        if (bResult)
        {
            AddLog(_T("인디케이터 → PLC: 데이터 전송 성공"));
        }
        else
        {
            AddLog(_T("인디케이터 → PLC: 데이터 전송 실패"));
        }
    }
    else
    {
        AddLog(_T("PLC 클라이언트 연결 안됨: 데이터 전송 실패"));
    }
}
void C250410MFCAppDlg::PLCToIndicator(const BYTE* pData, int nLength)
{
    if (m_bIndicatorConnected)
    {
        AddLog(_T("PLC → 인디케이터: "), pData, nLength);
        m_indicatorSocket.SendData(pData, nLength);
    }
    else
    {
        AddLog(_T("인디케이터 연결 안됨: 데이터 전송 실패"));
    }
}


void C250410MFCAppDlg::AddLog(LPCTSTR pszPrefix, const BYTE* pData, int nLength)
{
    CString strLog = pszPrefix;
    CString strTemp;

    // 16진수 형식으로 데이터 추가 (최대 32바이트만 표시)
    int nMaxDisplay = min(nLength, 32);
    for (int i = 0; i < nMaxDisplay; i++)
    {
        strTemp.Format(_T("%02X "), pData[i]);
        strLog += strTemp;
    }

    if (nLength > nMaxDisplay)
        strLog += _T("... ");

    strLog.Format(_T("%s [%d바이트]"), strLog, nLength);

    // 로그 리스트에 추가
    int nIndex = m_logList.AddString(strLog);
    m_logList.SetTopIndex(nIndex);
}

void C250410MFCAppDlg::AddLog(LPCTSTR pszLog)
{
    CTime time = CTime::GetCurrentTime();
    CString strTimedLog;
    strTimedLog.Format(_T("[%02d:%02d:%02d] %s"),
        time.GetHour(), time.GetMinute(), time.GetSecond(), pszLog);

    int nIndex = m_logList.AddString(strTimedLog);
    m_logList.SetTopIndex(nIndex);
}

CString C250410MFCAppDlg::AnalyzeModbusPacket(const BYTE* pData, int nLength)
{
    // 최소 Modbus-TCP 헤더 길이 확인
    if (nLength < 7)
        return _T("");

    CString strResult;

    // MBAP 헤더 분석
    WORD transactionID = (pData[0] << 8) | pData[1];
    WORD protocolID = (pData[2] << 8) | pData[3];
    WORD length = (pData[4] << 8) | pData[5];
    BYTE unitID = pData[6];

    strResult.Format(_T("Modbus-TCP: TrID=%04X, Length=%d, UnitID=%d"),
        transactionID, length, unitID);

    // 기능 코드 분석
    if (nLength >= 8)
    {
        BYTE functionCode = pData[7];
        CString strFunction;

        switch (functionCode)
        {
        case 0x03:
            strFunction = _T("Read Holding Registers");
            break;
        case 0x06:
            strFunction = _T("Write Single Register");
            break;
        case 0x10:
            strFunction = _T("Write Multiple Registers");
            break;
        default:
            strFunction.Format(_T("Function 0x%02X"), functionCode);
            break;
        }

        strResult.AppendFormat(_T(", Function=%s"), strFunction);

        // 기능별 추가 분석
        if (functionCode == 0x03 && nLength >= 12)  // Read Holding Registers
        {
            WORD startAddr = (pData[8] << 8) | pData[9];
            WORD regCount = (pData[10] << 8) | pData[11];
            strResult.AppendFormat(_T(", StartAddr=0x%04X, Count=%d"), startAddr, regCount);
        }
        else if (functionCode == 0x06 && nLength >= 12)  // Write Single Register
        {
            WORD regAddr = (pData[8] << 8) | pData[9];
            WORD regValue = (pData[10] << 8) | pData[11];
            strResult.AppendFormat(_T(", RegAddr=0x%04X, Value=0x%04X"), regAddr, regValue);
        }
        else if (functionCode == 0x10 && nLength >= 13)  // Write Multiple Registers
        {
            WORD startAddr = (pData[8] << 8) | pData[9];
            WORD regCount = (pData[10] << 8) | pData[11];
            BYTE byteCount = pData[12];
            strResult.AppendFormat(_T(", StartAddr=0x%04X, Count=%d, Bytes=%d"),
                startAddr, regCount, byteCount);
        }
    }

    return strResult;
}