#ifndef CWORDFILTER_H
#define CWORDFILTER_H

#include <vector>
#include "CString.h"

enum
{
	FILTER_CHECK_CHAT	= 0x1,
	FILTER_CHECK_PM		= 0x2,
	FILTER_CHECK_NICK	= 0x4,
	FILTER_CHECK_TOALL	= 0x8,
};

enum
{
	FILTER_POSITION_FULL	= 1,
	FILTER_POSITION_START	= 2,
	FILTER_POSITION_PART	= 3,
};

enum
{
	FILTER_ACTION_LOG		= 0x1,
	FILTER_ACTION_TELLRC	= 0x2,
	FILTER_ACTION_REPLACE	= 0x4,
	FILTER_ACTION_WARN		= 0x8,
	FILTER_ACTION_JAIL		= 0x10,
	FILTER_ACTION_BAN		= 0x20,
};

struct SWordFilterRule
{
	SWordFilterRule() : check(0),  wordPosition(0), action(0), precisionPercentage(true), precision(100) {}

	int check;
	CString match;
	int wordPosition;
	int action;
	bool precisionPercentage;
	int precision;
	CString warnMessage;
};

class TClient;
class TPlayer;
class CWordFilter
{
	public:
		CWordFilter(TClient* pServer) : server(pServer), showWordsToRC(false) {}
		~CWordFilter();

		void load(const CString& file);
		int apply(const TPlayer* player, CString& chat, int check);

	private:
		TClient* server;

		CString defaultWarnMessage;
		bool showWordsToRC;
		std::vector<SWordFilterRule*> rules;
};

#endif
