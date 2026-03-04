// Message.h: interface for the CMessage class.
//
//////////////////////////////////////////////////////////////////////

#pragma once

struct MESSAGE_INFO
{
	int Index;
	char Message[128];
};

class CMessage
{
public:
	CMessage();
	virtual ~CMessage();
	void Load(char* path, const char* language);
	char* GlobalText(int index);
private:
	bool LoadFromTxt(const char* path);
	bool LoadFromXml(const char* path);
	char m_DefaultMessage[128];
	std::map<int,MESSAGE_INFO> m_MessageInfo;
};

extern CMessage gMessage;
