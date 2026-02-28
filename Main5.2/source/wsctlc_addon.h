#ifndef _WSCTLC_ADDON_H_
#define _WSCTLC_ADDON_H_
#define MAX_SENDBUF			16384
#define MAX_RECVBUF			16384


class CPacket{
	BYTE	m_byBuffer[MAX_RECVBUF];
	int		m_size;

public:
	CPacket(BYTE* byBuf, int size) {
		memcpy(m_byBuffer, byBuf, size);
		m_byBuffer[size] = 0xFD;	//. �޸� ����ǥ��
		m_size = size;
	}
	~CPacket(){}

	int GetSize() const { return m_size; }
	BYTE* GetBuffer() { return m_byBuffer; }
};

class CPacketQueue
{
	class CGarbageCollection
	{
		std::list<CPacket*>		m_listGarbage;
		bool	m_bCheckIntegrity;
	public:
		CGarbageCollection() : m_bCheckIntegrity(false){}
		~CGarbageCollection(){ Clear(); }

		void Registry(CPacket* pPacket){
			m_listGarbage.push_back(pPacket);
		}
		void Unregistry(CPacket* pPacket){
			m_listGarbage.remove(pPacket);
		}
		
		void EnableCheckIntegrity() { m_bCheckIntegrity = true; }
		void DisableCheckIntegrity() { m_bCheckIntegrity = false; }

		void Clear(){
			if(!m_listGarbage.empty()){
				std::list<CPacket*>::iterator li = m_listGarbage.begin();
				for(; li != m_listGarbage.end(); li++){
#ifdef _DEBUG
					if(m_bCheckIntegrity){	//. �޸� ���Ἲ �˻�
						BYTE MemBlock = *((*li)->GetBuffer()+(*li)->GetSize());
						assert(MemBlock == 0xFD);
					}
#endif // _DEBUG
					delete (*li);
				}
				m_listGarbage.clear();
			}
		}
	};

	std::queue<CPacket*>	m_queuePacket;
	CGarbageCollection*		m_pGabageCollection;
	CRITICAL_SECTION		m_crit;

public:
	CPacketQueue(){
		m_pGabageCollection = new CGarbageCollection;
		InitializeCriticalSection(&m_crit);
#ifdef _DEBUG
		m_pGabageCollection->EnableCheckIntegrity();
#endif // _DEBUG
	}
	~CPacketQueue(){
		ClearGarbage();
		
		DeleteCriticalSection(&m_crit);

		while(!m_queuePacket.empty()){
			delete m_queuePacket.front();
			m_queuePacket.pop();
		}

		delete m_pGabageCollection;
	}

	void PushPacket(BYTE* byBuf, int size) {
		EnterCriticalSection(&m_crit);
		m_queuePacket.push(new CPacket(byBuf, size));
		LeaveCriticalSection(&m_crit);
	}
	void PopPacket(){
		EnterCriticalSection(&m_crit);
		if (!m_queuePacket.empty()) {
			m_pGabageCollection->Registry(m_queuePacket.front());
			m_queuePacket.pop();
		}
		LeaveCriticalSection(&m_crit);
	}
	bool IsEmpty() { 
		bool bEmpty;
		EnterCriticalSection(&m_crit);
		bEmpty = m_queuePacket.empty(); 
		LeaveCriticalSection(&m_crit);
		return bEmpty;
	}
	size_t GetQueueSize() { 
		size_t size;
		EnterCriticalSection(&m_crit);
		size = m_queuePacket.size(); 
		LeaveCriticalSection(&m_crit);
		return size;
	}


	CPacket* FrontPacket() {
		CPacket* packet = NULL;
		EnterCriticalSection(&m_crit);
		if (!m_queuePacket.empty()) {
			packet = m_queuePacket.front();
		}
		LeaveCriticalSection(&m_crit);
		return packet;
	}

	void ClearGarbage() { m_pGabageCollection->Clear(); }
};

#endif // _WSCTLC_ADDON_H_