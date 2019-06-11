//=========================================================
//    >> CNPC_CombineX
//=========================================================
#include "npc_combines.h"

class CNPC_CombineX : public CNPC_CombineS
{
	DECLARE_CLASS(CNPC_CombineX, CNPC_CombineS); 
public:
	void        Spawn();
	bool        IsJumpLegal(const Vector &startPos, const Vector &apex, const Vector &endPos) const;
};

