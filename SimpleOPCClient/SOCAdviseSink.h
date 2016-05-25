// Class for implementing the client-side IAdviseSink interface,
// largely based on the KEPWARE�s sample client code.
//
// Luiz T. S. Mendes - DELT/UFMG - 05/09/2011
// 

#include <ObjIdl.h>
#include <string>

// Class definition
#ifndef _SOCADVISESINK_H
#define _SOCADVISESINK_H

#define PRODUCTION 0
#define OEE 1
#define TIME_FINISH 2


class SOCAdviseSink : public IAdviseSink
	{
	public:
		SOCAdviseSink ();

		// IUnknown Methods
		HRESULT STDMETHODCALLTYPE QueryInterface (REFIID riid, LPVOID *ppv);
		ULONG STDMETHODCALLTYPE AddRef ();
		ULONG STDMETHODCALLTYPE Release ();

		// IAdviseSink Methods
		void STDMETHODCALLTYPE OnDataChange (FORMATETC *pFormatEtc, STGMEDIUM *pMedium); 
		void STDMETHODCALLTYPE OnViewChange (unsigned long dwAspect, long lindex) {/*Not implemented*/};
		void STDMETHODCALLTYPE OnRename (LPMONIKER pmk) {/*Not implemented*/};
		void STDMETHODCALLTYPE OnSave () {/*Not implemented*/};
		void STDMETHODCALLTYPE OnClose () {/*Not implemented*/};

		void SetProduction(char production[100]);
		void SetOEE(char oee[100]);
		void SetTimeToFinish(char time[100]);
		void RetrieveValues(char *production, char *oee, char *time);

	private: 

	protected:
		ULONG m_cRef; 
	};
#endif // _SOCADVISESINK_H