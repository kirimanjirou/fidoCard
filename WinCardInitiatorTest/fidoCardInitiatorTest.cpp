// sample_pcsc_04_Initiator.cpp
//

#include <windows.h>
#include <winscard.h>
#include <stdio.h>
#include <tchar.h>
#include <assert.h>

#pragma comment (lib, "winscard.lib")

#define SAMPLE_TITLE		_T("FIDO CARD (Initiator)")
#define PCSC_TRANS_BUFF_LEN	(262)
#define PCSC_RECV_BUFF_LEN	(262)
#define CONNECT_TIMEOUT		(10)

typedef struct {
	INT		iLength;
	BYTE	bCommand[PCSC_TRANS_BUFF_LEN];
} SENDCOMM;


SCARD_IO_REQUEST *CardProtocol2PCI(DWORD dwProtocol)
{
	if (dwProtocol == SCARD_PROTOCOL_T0) {
		return (SCARD_IO_REQUEST *)SCARD_PCI_T0;
	}
	else if (dwProtocol == SCARD_PROTOCOL_T1) {
		return (SCARD_IO_REQUEST *)SCARD_PCI_T1;
	}
	else if (dwProtocol == SCARD_PROTOCOL_RAW) {
		return (SCARD_IO_REQUEST *)SCARD_PCI_RAW;
	}
	else if (dwProtocol == SCARD_PROTOCOL_UNDEFINED) {
		assert(false);
		return NULL;
	}

	return (SCARD_IO_REQUEST *)SCARD_PCI_T1;
}

int main(int argc, _TCHAR* argv[])
{
	_ftprintf_s(stdout, _T("%s\n"), SAMPLE_TITLE);

	SCARDCONTEXT	hContext = 0;
	LONG lResult = ::SCardEstablishContext(SCARD_SCOPE_USER, NULL, NULL, &hContext);
	if (lResult != SCARD_S_SUCCESS) {
		if (lResult == SCARD_E_NO_SERVICE) {
			_ftprintf_s(stdout, _T("Smart Card Servise is not Started.\n"));
		}
		else {
			_ftprintf_s(stdout, _T("SCardEstablishContext Error.\nErrorCode %08X\n"), lResult);
		}
		return EXIT_FAILURE;
	}

	LPTSTR	lpszReaderName = NULL;
	DWORD	dwAutoAllocate = SCARD_AUTOALLOCATE;
	TCHAR	*pszExpectedReaderName = _T("Sony FeliCa Port/PaSoRi 3.0");
	lResult = ::SCardListReaders(hContext, NULL, (LPTSTR)&lpszReaderName, &dwAutoAllocate);

	if (lResult != SCARD_S_SUCCESS) {
		if (lResult == SCARD_E_NO_READERS_AVAILABLE) {
			_ftprintf_s(stdout, _T("Reader/Writer is not Found.\n"));
		}
		else {
			_ftprintf_s(stdout, _T("SCardListReaders Error.\nErrorCode %08X\n"), lResult);
		}
		::SCardReleaseContext(hContext);
		return EXIT_FAILURE;
	}
	if (_tcsncmp(pszExpectedReaderName, lpszReaderName, _tcslen(pszExpectedReaderName)) != 0) {
		_ftprintf_s(stdout, _T("Reader/Writer is not Found.\n"));
		::SCardFreeMemory(hContext, lpszReaderName);
		::SCardReleaseContext(hContext);
		return EXIT_FAILURE;
	}

	SCARDHANDLE	hCard = NULL;
	DWORD		dwActiveProtocol = 0;
	for (UINT uiTryTime = 0; uiTryTime <= CONNECT_TIMEOUT; uiTryTime++) {
		lResult = ::SCardConnect(hContext, lpszReaderName, SCARD_SHARE_SHARED, SCARD_PROTOCOL_T0 | SCARD_PROTOCOL_T1, &hCard, &dwActiveProtocol);
		if (lResult == SCARD_S_SUCCESS) {
			break;
		}
		else if (lResult == SCARD_W_REMOVED_CARD) {
			_ftprintf_s(stdout, _T("%3d\r"), CONNECT_TIMEOUT - uiTryTime);
			if ((CONNECT_TIMEOUT - uiTryTime) > 0) {
				Sleep(1000);
			}
			else {
				_ftprintf_s(stdout, _T("Card is not Found.\n"));

				::SCardFreeMemory(hContext, lpszReaderName);
				::SCardReleaseContext(hContext);
				return EXIT_FAILURE;
			}
		}
		else {
			_ftprintf_s(stdout, _T("SCardConnect Error.\nErrorCode %08X\n"), lResult);

			::SCardFreeMemory(hContext, lpszReaderName);
			::SCardReleaseContext(hContext);
			return EXIT_FAILURE;
		}
	}

	SENDCOMM	SendComm[] = {
		/* APDU SELECT AID */
		{ 13,{ 0x00, 0xA4, 0x04, 0x00, 0x08,
			   0xA0, 0x00, 0x00, 0x06, 0x47, 0x2F, 0x00, 0x01, } },	
		/* APDU Register Request(challenge parameter & application parameter) */
	    { 69,{ 0x00, 0x01, 0x03, 0x00, 0x40,
			   0x01, 0x02, 0x03, 0x04, 0x05, 0x06, 0x07, 0x08,
			   0x01, 0x02, 0x03, 0x04, 0x05, 0x06, 0x07, 0x08,
			   0x01, 0x02, 0x03, 0x04, 0x05, 0x06, 0x07, 0x08,
			   0x01, 0x02, 0x03, 0x04, 0x05, 0x06, 0x07, 0x08,
			   0x01, 0x02, 0x03, 0x04, 0x05, 0x06, 0x07, 0x08,
			   0x01, 0x02, 0x03, 0x04, 0x05, 0x06, 0x07, 0x08,
			   0x01, 0x02, 0x03, 0x04, 0x05, 0x06, 0x07, 0x08, } },	
		{ -1, NULL },
	};
	BYTE		bRecvBuf[PCSC_RECV_BUFF_LEN] = { 0x00 };
	DWORD		dwResponseSize;
	for (UINT uiCmdIdx = 0; SendComm[uiCmdIdx].iLength > -1; uiCmdIdx++) {
		dwResponseSize = sizeof(bRecvBuf);
		lResult = ::SCardTransmit(hCard, CardProtocol2PCI(dwActiveProtocol),
			SendComm[uiCmdIdx].bCommand, SendComm[uiCmdIdx].iLength, NULL, bRecvBuf, &dwResponseSize);
		if (lResult != SCARD_S_SUCCESS) {
			_ftprintf_s(stdout, _T("SCardTransmit Error.\nErrorCode:0x%08X\n"), lResult);

			::SCardDisconnect(hCard, SCARD_LEAVE_CARD);
			::SCardFreeMemory(hContext, lpszReaderName);
			::SCardReleaseContext(hContext);
			return EXIT_FAILURE;
		}

		for (UINT uiRespIdx = 0; uiRespIdx < dwResponseSize; uiRespIdx++) {
			_ftprintf_s(stdout, _T("%02X"), bRecvBuf[uiRespIdx]);
			if ((uiRespIdx + 1) >= dwResponseSize) {
				_ftprintf_s(stdout, _T("\n"));
			}
			else {
				_ftprintf_s(stdout, _T(" "));
			}
		}
	}

	::SCardDisconnect(hCard, SCARD_LEAVE_CARD);
	::SCardFreeMemory(hContext, lpszReaderName);
	::SCardReleaseContext(hContext);

	return EXIT_SUCCESS;
}
