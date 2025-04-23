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
#define WM_SOCKET_ACCEPT  (WM_USER + 4)

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
{
    m_hIcon = AfxGetApp()->LoadIcon(IDR_MAINFRAME);
}

C250410MFCAppDlg::~C250410MFCAppDlg()
{
}

void C250410MFCAppDlg::DoDataExchange(CDataExchange* pDX)
{
    CDialogEx::DoDataExchange(pDX);
    DDX_Control(pDX, IDC_LIST_CONNECTION, m_connectionList);
    DDX_Control(pDX, IDC_LIST_LOG, m_logList);
    DDX_Control(pDX, IDC_BTN_CONNECT_ALL, m_btnConnectAll);
    DDX_Control(pDX, IDC_BTN_CLEAR_LOG, m_btnClearLog);
}

BEGIN_MESSAGE_MAP(C250410MFCAppDlg, CDialogEx)
    ON_WM_PAINT()
    ON_WM_QUERYDRAGICON()
    ON_BN_CLICKED(IDC_BTN_CONNECT_ALL, &C250410MFCAppDlg::OnBnClickedBtnConnectAll)
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
    // 버튼 텍스트 설정
    m_btnConnectAll.SetWindowText(_T("모든 연결 시작"));
    m_btnClearLog.SetWindowText(_T("로그 지우기"));

    // 연결 개수 초기화
    m_connectionCount = 0;

    // set.csv 파일 로드
    if (LoadConnectionsFromFile(_T("set.csv")))
    {
        AddLog(_T("set.csv 파일 로드 성공"));
        InitializeConnections();
    }
    else
    {
        AddLog(_T("set.csv 파일 로드 실패, 기본 연결 생성"));

        // 기본 연결 생성 (배열 첫 번째 요소에 저장)
        m_connections[0].indicatorIP = _T("127.0.0.1");
        m_connections[0].indicatorPort = 5020;
        m_connections[0].unitID = 1;
        m_connections[0].plcPort = 0;  // 자동 할당
        m_connections[0].indicatorConnected = false;
        m_connections[0].plcConnected = false;

        // 연결 개수 설정
        m_connectionCount = 1;

        InitializeConnections();
    }

    // 로그 초기화
    AddLog(_T("ModBus-TCP 릴레이 프로그램 시작"));
    AddLog(_T("여러 인디케이터와 PLC 간의 통신을 중계합니다."));

    // 상태 표시 업데이트
    UpdateStatusDisplay();

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

BOOL C250410MFCAppDlg::LoadConnectionsFromFile(LPCTSTR lpszFilePath)
{
    CStdioFile file;
    CFileException fileException;

    m_connectionCount = 0;  // 연결 개수 초기화

    if (!file.Open(lpszFilePath, CFile::modeRead | CFile::typeText, &fileException))
    {
        CString strError;
        strError.Format(_T("CSV 파일을 열 수 없습니다: %s"), lpszFilePath);
        AddLog(strError);
        return FALSE;
    }

    // 첫 줄은 헤더이므로 건너뜁니다
    CString strLine;
    file.ReadString(strLine);

    // 연결 쌍 정보 읽기
    while (file.ReadString(strLine) && m_connectionCount < MAX_CONNECTIONS)
    {
        // CSV 파싱
        int pos = 0;
        CString token = strLine.Tokenize(_T(","), pos);
        m_connections[m_connectionCount].indicatorIP = token;

        token = strLine.Tokenize(_T(","), pos);
        m_connections[m_connectionCount].indicatorPort = _ttoi(token);

        token = strLine.Tokenize(_T(","), pos);
        m_connections[m_connectionCount].unitID = _ttoi(token);

        // PLC 서버 포트는 자동 할당 (나중에 초기화)
        m_connections[m_connectionCount].plcPort = 0;

        // 소켓 및 연결 상태 초기화
        m_connections[m_connectionCount].indicatorConnected = false;
        m_connections[m_connectionCount].plcConnected = false;

        // 연결 개수 증가
        m_connectionCount++;
    }

    file.Close();
    return (m_connectionCount > 0);
}

void C250410MFCAppDlg::InitializeConnections()
{
    // 기본 포트 시작 번호
    int nBasePort = 5030;

    // 각 연결 쌍에 대해 소켓 초기화 및 포트 할당
    for (int i = 0; i < m_connectionCount; i++)
    {
        ConnectionPair& pair = m_connections[i];

        // 인디케이터 소켓 설정
        pair.indicatorSocket.SetParent(this);
        pair.indicatorSocket.SetSocketID(SOCKET_INDICATOR + (i * 10));  // 고유 ID 부여

        // 서버 소켓 설정
        pair.serverSocket.SetParent(this);
        pair.serverSocket.SetSocketID(SOCKET_SERVER + (i * 10));  // 고유 ID 부여
        pair.serverSocket.SetListenMode(true);

        // 클라이언트 소켓 설정
        pair.clientSocket.SetParent(this);
        pair.clientSocket.SetSocketID(SOCKET_CLIENT + (i * 10));  // 고유 ID 부여

        // PLC 서버 포트 할당
        int nFoundPort = FindAvailablePort(nBasePort, nBasePort + 100);
        pair.plcPort = nFoundPort;
        nBasePort = nFoundPort + 1;  // 다음 포트 검색 시작점

        CString strLog;
        strLog.Format(_T("인디케이터 %s:%d(Unit ID: %d) - PLC 서버 포트: %d 할당됨"),
            pair.indicatorIP, pair.indicatorPort, pair.unitID, pair.plcPort);
        AddLog(strLog);
    }

    // UI 업데이트
    UpdateConnectionList();
}

void C250410MFCAppDlg::OnBnClickedBtnConnectAll()
{
    // 모든 연결 시작 또는 중지
    bool allConnected = true;
    for (int i = 0; i < m_connectionCount; i++)
    {
        if (!m_connections[i].indicatorConnected || !m_connections[i].plcConnected)
        {
            allConnected = false;
            break;
        }
    }
    if (!allConnected)
    {
        // 모든 연결 시작
        AddLog(_T("모든 연결 시작..."));
        for (int i = 0; i < m_connectionCount; i++)
        {
            ConnectionPair& pair = m_connections[i];
            // 인디케이터 연결
            if (!pair.indicatorConnected)
            {
                ConnectIndicator(i);
            }
            // PLC 서버 시작
            if (!pair.plcConnected)
            {
                StartPLCServer(i);
            }
        }
        m_btnConnectAll.SetWindowText(_T("모든 연결 중지"));
    }
    else
    {
        // 모든 연결 중지
        AddLog(_T("모든 연결 중지..."));
        for (int i = 0; i < m_connectionCount; i++)
        {
            ConnectionPair& pair = m_connections[i];
            // 인디케이터 연결 해제
            if (pair.indicatorConnected)
            {
                pair.indicatorSocket.Close();
                pair.indicatorConnected = false;
            }
            // PLC 서버 중지
            if (pair.plcConnected)
            {
                pair.serverSocket.Close();
                pair.clientSocket.Close();
                pair.plcConnected = false;
            }
        }
        m_btnConnectAll.SetWindowText(_T("모든 연결 시작"));
    }
    UpdateConnectionList();
    UpdateStatusDisplay();
}
void C250410MFCAppDlg::ConnectIndicator(int index)
{
    ConnectionPair& pair = m_connections[index];

    // 인디케이터 연결
    if (pair.indicatorSocket.Create())
    {
        if (pair.indicatorSocket.Connect(pair.indicatorIP, pair.indicatorPort))
        {
            CString strLog;
            strLog.Format(_T("인디케이터 %s:%d 연결 중..."), pair.indicatorIP, pair.indicatorPort);
            AddLog(strLog);
        }
        else
        {
            int nError = GetLastError();
            if (nError != WSAEWOULDBLOCK)
            {
                CString strError;
                strError.Format(_T("인디케이터 %s:%d 연결 실패: 오류 코드 %d"),
                    pair.indicatorIP, pair.indicatorPort, nError);
                AddLog(strError);
                pair.indicatorSocket.Close();
            }
        }
    }
    else
    {
        CString strError;
        strError.Format(_T("인디케이터 %s:%d 소켓 생성 실패"), pair.indicatorIP, pair.indicatorPort);
        AddLog(strError);
    }
}

void C250410MFCAppDlg::StartPLCServer(int index)
{
    ConnectionPair& pair = m_connections[index];

    // PLC 서버 시작
    if (pair.serverSocket.Create(pair.plcPort))
    {
        if (pair.serverSocket.Listen())
        {
            pair.plcConnected = true;

            CString strLog;
            strLog.Format(_T("PLC 서버 모드 시작: 포트 %d 대기 중 (인디케이터 %s:%d 용)"),
                pair.plcPort, pair.indicatorIP, pair.indicatorPort);
            AddLog(strLog);
        }
        else
        {
            int nError = GetLastError();
            CString strError;
            strError.Format(_T("PLC 서버 시작 실패: 포트 %d, 오류 코드 %d"), pair.plcPort, nError);
            AddLog(strError);
            pair.serverSocket.Close();
        }
    }
    else
    {
        CString strError;
        strError.Format(_T("PLC 서버 소켓 생성 실패: 포트 %d"), pair.plcPort);
        AddLog(strError);
    }
}

void C250410MFCAppDlg::OnBnClickedBtnClearLog()
{
    m_logList.ResetContent();
    AddLog(_T("로그 지움"));
}

int C250410MFCAppDlg::FindConnectionBySocketID(int nSocketID)
{
    // 해당 소켓 ID가 어떤 연결 쌍에 속하는지 찾기
    int baseID = nSocketID / 10 * 10;  // 소켓 ID에서 기본 ID 추출
    int index = baseID / 10;

    if (index >= 0 && index < (int)m_connectionCount)
    {
        return index;
    }

    return -1;  // 찾지 못함
}

void C250410MFCAppDlg::UpdateConnectionList()
{
    // 연결 목록 업데이트
    m_connectionList.ResetContent();

    for (size_t i = 0; i < m_connectionCount; i++)
    {
        ConnectionPair& pair = m_connections[i];

        CString strStatus;
        CString strIndicatorStatus = pair.indicatorConnected ? _T("연결됨") : _T("연결 안됨");
        CString strPLCStatus = pair.plcConnected ? (pair.clientSocket.IsConnected() ? _T("연결됨") : _T("대기 중")) : _T("중지됨");

        strStatus.Format(_T("%d. %s:%d(Unit:%d) [%s] <-> PLC 포트:%d [%s]"),
            i + 1, pair.indicatorIP, pair.indicatorPort, pair.unitID,
            strIndicatorStatus, pair.plcPort, strPLCStatus);

        m_connectionList.AddString(strStatus);
    }
}

void C250410MFCAppDlg::UpdateStatusDisplay()
{
    // 상태 표시 업데이트
    CString strStatus;
    int nConnectedInd = 0, nConnectedPLC = 0;

    for (size_t i = 0; i < m_connectionCount; i++)
    {
        if (m_connections[i].indicatorConnected) nConnectedInd++;
        if (m_connections[i].plcConnected) nConnectedPLC++;
    }

    strStatus.Format(_T("ModBus-TCP 릴레이 프로그램 - 연결 현황: 인디케이터 %d/%d, PLC %d/%d"),
        nConnectedInd, m_connectionCount, nConnectedPLC, m_connectionCount);

    SetWindowText(strStatus);
}

LRESULT C250410MFCAppDlg::OnSocketConnect(WPARAM wParam, LPARAM lParam)
{
    int nSocketID = (int)wParam;
    int nErrorCode = (int)lParam;

    // 어떤 연결 쌍에 속하는지 찾기
    int index = FindConnectionBySocketID(nSocketID);
    if (index < 0) return 0;

    ConnectionPair& pair = m_connections[index];

    // 소켓 타입 확인
    int baseID = nSocketID / 10 * 10;
    int socketType = nSocketID - baseID;

    if (socketType == SOCKET_INDICATOR)
    {
        if (nErrorCode == 0)
        {
            pair.indicatorConnected = true;
            CString strLog;
            strLog.Format(_T("인디케이터 %s:%d 연결 성공"), pair.indicatorIP, pair.indicatorPort);
            AddLog(strLog);
        }
        else
        {
            CString strError;
            strError.Format(_T("인디케이터 %s:%d 연결 실패: 오류 코드 %d"),
                pair.indicatorIP, pair.indicatorPort, nErrorCode);
            AddLog(strError);
            pair.indicatorSocket.Close();
        }
    }
    else if (socketType == SOCKET_CLIENT)
    {
        // 클라이언트 소켓 연결은 OnSocketAccept에서 처리
    }

    UpdateConnectionList();
    UpdateStatusDisplay();

    return 0;
}

LRESULT C250410MFCAppDlg::OnSocketAccept(WPARAM wParam, LPARAM lParam)
{
    int nSocketID = (int)wParam;

    // 어떤 연결 쌍에 속하는지 찾기
    int index = FindConnectionBySocketID(nSocketID);
    if (index < 0) return 0;

    ConnectionPair& pair = m_connections[index];

    // 소켓 타입 확인
    int baseID = nSocketID / 10 * 10;
    int socketType = nSocketID - baseID;

    if (socketType == SOCKET_SERVER)
    {
        // 새로운 클라이언트 연결 수락
        if (pair.clientSocket.m_hSocket != INVALID_SOCKET)
        {
            // 기존 연결이 있으면 닫기
            pair.clientSocket.Close();
        }

        // 연결 수락
        SOCKADDR_IN clientAddr;
        int addrLen = sizeof(clientAddr);

        if (pair.serverSocket.Accept(pair.clientSocket, (SOCKADDR*)&clientAddr, &addrLen))
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
            strInfo.Format(_T("포트 %d에 클라이언트 접속: %s:%d (인디케이터 %s:%d 용)"),
                pair.plcPort, strClientIP, nClientPort, pair.indicatorIP, pair.indicatorPort);
            AddLog(strInfo);

            // 클라이언트 소켓 설정
            pair.clientSocket.SetSocketID(SOCKET_CLIENT + (index * 10));
            pair.clientSocket.m_bConnected = true;
        }
        else
        {
            CString strError;
            strError.Format(_T("포트 %d 클라이언트 연결 수락 실패"), pair.plcPort);
            AddLog(strError);
        }
    }

    UpdateConnectionList();
    UpdateStatusDisplay();

    return 0;
}

LRESULT C250410MFCAppDlg::OnSocketReceive(WPARAM wParam, LPARAM lParam)
{
    int nSocketID = (int)wParam;
    int nBytesAvailable = (int)lParam;

    // 어떤 연결 쌍에 속하는지 찾기
    int index = FindConnectionBySocketID(nSocketID);
    if (index < 0) return 0;

    ConnectionPair& pair = m_connections[index];

    // 소켓 타입 확인
    int baseID = nSocketID / 10 * 10;
    int socketType = nSocketID - baseID;

    if (socketType == SOCKET_INDICATOR)
    {
        // 인디케이터에서 데이터 수신
        std::vector<BYTE>& buffer = pair.indicatorSocket.m_RecvBuffer;
        if (!buffer.empty())
        {
            // Modbus-TCP 패킷 분석 및 로그
            CString strAnalysis = AnalyzeModbusPacket(buffer.data(), (int)buffer.size());
            if (!strAnalysis.IsEmpty())
            {
                CString strFullLog;
                strFullLog.Format(_T("[인디케이터 %s:%d] %s"),
                    pair.indicatorIP, pair.indicatorPort, strAnalysis);
                AddLog(strFullLog);
            }

            // PLC로 데이터 전달
            IndicatorToPLC(index, buffer.data(), (int)buffer.size());

            // 버퍼 비우기
            buffer.clear();
        }
    }
    else if (socketType == SOCKET_CLIENT)
    {
        // 클라이언트(PLC)에서 데이터 수신
        std::vector<BYTE>& buffer = pair.clientSocket.m_RecvBuffer;
        if (!buffer.empty())
        {
            // Modbus-TCP 패킷 분석 및 로그
            CString strAnalysis = AnalyzeModbusPacket(buffer.data(), (int)buffer.size());
            if (!strAnalysis.IsEmpty())
            {
                CString strFullLog;
                strFullLog.Format(_T("[PLC 포트 %d] %s"), pair.plcPort, strAnalysis);
                AddLog(strFullLog);
            }

            // 인디케이터로 데이터 전달
            PLCToIndicator(index, buffer.data(), (int)buffer.size());

            // 버퍼 비우기
            buffer.clear();
        }
    }

    return 0;
}

LRESULT C250410MFCAppDlg::OnSocketClose(WPARAM wParam, LPARAM lParam)
{
    int nSocketID = (int)wParam;

    // 어떤 연결 쌍에 속하는지 찾기
    int index = FindConnectionBySocketID(nSocketID);
    if (index < 0) return 0;

    ConnectionPair& pair = m_connections[index];

    // 소켓 타입 확인
    int baseID = nSocketID / 10 * 10;
    int socketType = nSocketID - baseID;

    if (socketType == SOCKET_INDICATOR)
    {
        pair.indicatorConnected = false;
        CString strLog;
        strLog.Format(_T("인디케이터 %s:%d 연결 종료됨"), pair.indicatorIP, pair.indicatorPort);
        AddLog(strLog);
    }
    else if (socketType == SOCKET_CLIENT)
    {
        CString strLog;
        strLog.Format(_T("PLC 포트 %d 클라이언트 연결 종료됨"), pair.plcPort);
        AddLog(strLog);
    }

    UpdateConnectionList();
    UpdateStatusDisplay();

    return 0;
}

void C250410MFCAppDlg::IndicatorToPLC(int index, const BYTE* pData, int nLength)
{
    ConnectionPair& pair = m_connections[index];

    if (pair.clientSocket.m_hSocket != INVALID_SOCKET && pair.clientSocket.IsConnected())
    {
        CString strLog;
        strLog.Format(_T("인디케이터 %s:%d → PLC 포트 %d: "),
            pair.indicatorIP, pair.indicatorPort, pair.plcPort);
        AddLog(strLog, pData, nLength);

        BOOL bResult = pair.clientSocket.SendData(pData, nLength);

        if (!bResult)
        {
            CString strError;
            strError.Format(_T("인디케이터 %s:%d → PLC 포트 %d: 데이터 전송 실패"),
                pair.indicatorIP, pair.indicatorPort, pair.plcPort);
            AddLog(strError);
        }
    }
    else
    {
        CString strError;
        strError.Format(_T("PLC 포트 %d 클라이언트 연결 안됨: 데이터 전송 실패"), pair.plcPort);
        AddLog(strError);
    }
}

void C250410MFCAppDlg::PLCToIndicator(int index, const BYTE* pData, int nLength)
{
    ConnectionPair& pair = m_connections[index];

    if (pair.indicatorConnected)
    {
        CString strLog;
        strLog.Format(_T("PLC 포트 %d → 인디케이터 %s:%d: "),
            pair.plcPort, pair.indicatorIP, pair.indicatorPort);
        AddLog(strLog, pData, nLength);

        BOOL bResult = pair.indicatorSocket.SendData(pData, nLength);

        if (!bResult)
        {
            CString strError;
            strError.Format(_T("PLC 포트 %d → 인디케이터 %s:%d: 데이터 전송 실패"),
                pair.plcPort, pair.indicatorIP, pair.indicatorPort);
            AddLog(strError);
        }
    }
    else
    {
        CString strError;
        strError.Format(_T("인디케이터 %s:%d 연결 안됨: 데이터 전송 실패"),
            pair.indicatorIP, pair.indicatorPort);
        AddLog(strError);
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

int C250410MFCAppDlg::FindAvailablePort(int nStartPort, int nEndPort)
{
    for (int port = nStartPort; port <= nEndPort; port++)
    {
        CAsyncSocket testSocket;
        if (testSocket.Create(port))
        {
            testSocket.Close();
            return port;  // 포트 사용 가능
        }
        else
        {
            int nError = GetLastError();
            if (nError != WSAEADDRINUSE)
            {
                // 주소가 이미 사용 중인 경우가 아닌 다른 오류라면
                // 기본 포트로 돌아감
                return nStartPort;
            }
        }
    }
    // 모든 포트가 사용 중이면 기본 포트 반환
    return nStartPort;
}