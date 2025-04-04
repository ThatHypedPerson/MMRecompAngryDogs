#include "global.h"
#include "modding.h"

#include "recompconfig.h"
#include "recomputils.h"

struct EnDg;

typedef void (*EnDgActionFunc)(struct EnDg*, PlayState*);

#define ENDG_GET_INDEX(thisx) (((thisx)->params & 0x3E0) >> 5)
#define ENDG_GET_PATH_INDEX(thisx) (((thisx)->params & 0xFC00) >> 10)
#define ENDG_PARAMS(path, index) ((path << 10) | (index << 5))

#define ENDG_PATH_INDEX_NONE 0x3F

#define DOG_FLAG_NONE 0
#define DOG_FLAG_HELD (1 << 0)
#define DOG_FLAG_JUMP_ATTACKING (1 << 1)
#define DOG_FLAG_SWIMMING (1 << 2)
#define DOG_FLAG_BOUNCED (1 << 3)
#define DOG_FLAG_THROWN (1 << 4)
#define DOG_FLAG_FOLLOWING_BREMEN_MASK (1 << 5)

#define ENDG_INDEX_NO_BREMEN_MASK_FOLLOWER 99

#define DOG_LIMB_MAX 0x0D

typedef struct EnDg {
    /* 0x000 */ Actor actor;
    /* 0x144 */ EnDgActionFunc actionFunc;
    /* 0x148 */ UNK_TYPE1 unk_148[0x4];
    /* 0x14C */ SkelAnime skelAnime;
    /* 0x190 */ ColliderCylinder collider;
    /* 0x1DC */ Path* path;
    /* 0x1E0 */ s32 currentPoint;
    /* 0x1E4 */ Vec3s jointTable[DOG_LIMB_MAX];
    /* 0x232 */ Vec3s morphTable[DOG_LIMB_MAX];
    /* 0x280 */ u16 dogFlags;
    /* 0x282 */ s16 timer;
    /* 0x284 */ s16 swimTimer;
    /* 0x286 */ s16 index;
    /* 0x288 */ s16 selectedDogIndex;
    /* 0x28A */ s16 sitAfterThrowTimer;
    /* 0x28C */ s16 behavior;
    /* 0x28E */ s16 attackTimer;
    /* 0x290 */ s16 grabState;
    /* 0x292 */ s16 bremenBarkTimer;
    /* 0x294 */ Vec3f curRot;
} EnDg; // size = 0x2A0

typedef struct {
    /* 0x0 */ s16 color;  // The dog's color, which is used as an index into sBaseSpeeds
    /* 0x2 */ s16 index;  // The dog's index within sDogInfo
    /* 0x4 */ s16 textId; // The ID of the text to display when the dog is picked up
} RacetrackDogInfo;       // size = 0x6

typedef enum {
    /* 0 */ DOG_BEHAVIOR_INITIAL, // Gets immediately replaced by DOG_BEHAVIOR_DEFAULT in EnDg_Update
    /* 1 */ DOG_BEHAVIOR_HUMAN,   // Gets immediately replaced by DOG_BEHAVIOR_DEFAULT in EnDg_Update
    /* 2 */ DOG_BEHAVIOR_GORON,
    /* 3 */ DOG_BEHAVIOR_GORON_WAIT,
    /* 4 */ DOG_BEHAVIOR_ZORA,
    /* 5 */ DOG_BEHAVIOR_ZORA_WAIT,
    /* 6 */ DOG_BEHAVIOR_DEKU,
    /* 7 */ DOG_BEHAVIOR_DEKU_WAIT,
    /* 8 */ DOG_BEHAVIOR_DEFAULT
} DogBehavior;

typedef enum {
    /*  0 */ DOG_ANIM_WALK_AFTER_TALKING,
    /*  1 */ DOG_ANIM_WALK,
    /*  2 */ DOG_ANIM_RUN,
    /*  3 */ DOG_ANIM_BARK,
    /*  4 */ DOG_ANIM_SIT_DOWN_ONCE, // unused
    /*  5 */ DOG_ANIM_SIT_DOWN,
    /*  6 */ DOG_ANIM_LYING_DOWN_START_1, // unused
    /*  7 */ DOG_ANIM_LYING_DOWN_LOOP,    // unused
    /*  8 */ DOG_ANIM_LYING_DOWN_START_2, // unused
    /*  9 */ DOG_ANIM_LYING_DOWN_START_3, // unused
    /* 10 */ DOG_ANIM_LYING_DOWN_START_4, // unused
    /* 11 */ DOG_ANIM_WALK_BACKWARDS,
    /* 12 */ DOG_ANIM_JUMP,
    /* 13 */ DOG_ANIM_LONG_JUMP, // unused
    /* 14 */ DOG_ANIM_JUMP_ATTACK,
    /* 15 */ DOG_ANIM_SWIM,
    /* 16 */ DOG_ANIM_MAX
} DogAnimation;

typedef enum {
    /* 0 */ DOG_GRAB_STATE_NONE,
    /* 1 */ DOG_GRAB_STATE_HELD,
    /* 2 */ DOG_GRAB_STATE_THROWN_OR_SITTING_AFTER_THROW
} DogGrabState;

void EnDg_BackAwayFromGoron(EnDg *this, PlayState *play);
void EnDg_ApproachPlayerToAttack(EnDg *this, PlayState *play);
void EnDg_JumpAttack(EnDg *this, PlayState *play);
void EnDg_ApproachPlayer(EnDg *this, PlayState *play);
void EnDg_ChangeAnim(SkelAnime *skelAnime, AnimationInfoS *animInfo, s32 animIndex);
void EnDg_UpdateCollision(EnDg *this, PlayState *play);
void EnDg_GetFloorRot(EnDg *this, Vec3f *floorRot);
void EnDg_SetupIdleMove(EnDg *this, PlayState *play);
s32 EnDg_ShouldReactToNonHumanPlayer(EnDg *this, PlayState *play);
void EnDg_ChooseActionForForm(EnDg *this, PlayState *play);
void EnDg_SetupSwim(EnDg *this, PlayState *play);
void EnDg_TryPickUp(EnDg* this, PlayState* play);

extern RacetrackDogInfo sSelectedRacetrackDogInfo;
extern AnimationInfoS sAnimationInfo[DOG_ANIM_MAX];

typedef enum {
    /* 0 */ OPTION_NONE,
    /* 1 */ OPTION_HUMAN,
    /* 2 */ OPTION_DEKU,
    /* 3 */ OPTION_GORON,
    /* 4 */ OPTION_ZORA,
} DogBehaviorOption;

u32 lastOption;

bool EnDg_ShouldOverrideAction(EnDg* this, PlayState* play) {
    Player* player = GET_PLAYER(play);

    switch (recomp_get_config_u32("dog_behavior")) {
        case OPTION_HUMAN:
            return true;
        case OPTION_DEKU:
            if (this->actor.xzDistToPlayer < 250.0f) {
                return true;
            }
            return false;
        case OPTION_GORON:
        case OPTION_ZORA:
            if (this->actor.xzDistToPlayer < 300.0f) {
                return true;
            }
            return false;
        default:
            break;
    }

    return false;
}

void EnDg_ChooseActionForOption(EnDg* this, PlayState* play) {
    Player* player = GET_PLAYER(play);

    if (!(this->actor.bgCheckFlags & BGCHECKFLAG_WATER)) {
        switch (recomp_get_config_u32("dog_behavior")) {
            case OPTION_HUMAN:
                if (this->behavior != DOG_BEHAVIOR_HUMAN) {
                    this->behavior = DOG_BEHAVIOR_HUMAN;
                    this->dogFlags &= ~DOG_FLAG_JUMP_ATTACKING;
                    EnDg_SetupIdleMove(this, play);
                }
                break;

            case OPTION_ZORA:
                this->dogFlags &= ~DOG_FLAG_JUMP_ATTACKING;
                if ((this->behavior != DOG_BEHAVIOR_ZORA) && (player->actor.speed > 1.0f)) {
                    this->behavior = DOG_BEHAVIOR_ZORA;
                    EnDg_ChangeAnim(&this->skelAnime, sAnimationInfo, DOG_ANIM_RUN);
                    this->actionFunc = EnDg_ApproachPlayer;
                }

                if ((this->behavior != DOG_BEHAVIOR_ZORA_WAIT) && (this->behavior != DOG_BEHAVIOR_ZORA)) {
                    this->behavior = DOG_BEHAVIOR_ZORA_WAIT;
                    EnDg_SetupIdleMove(this, play);
                }
                break;

            case OPTION_GORON:
                this->dogFlags &= ~DOG_FLAG_JUMP_ATTACKING;
                if ((this->behavior != DOG_BEHAVIOR_GORON) && (player->actor.speed > 1.0f)) {
                    this->behavior = DOG_BEHAVIOR_GORON;
                    EnDg_ChangeAnim(&this->skelAnime, sAnimationInfo, DOG_ANIM_WALK_BACKWARDS);
                    this->timer = 50;
                    this->actionFunc = EnDg_BackAwayFromGoron;
                }

                if ((this->behavior != DOG_BEHAVIOR_GORON_WAIT) && (this->behavior != DOG_BEHAVIOR_GORON)) {
                    this->behavior = DOG_BEHAVIOR_GORON_WAIT;
                    EnDg_SetupIdleMove(this, play);
                }
                break;

            case OPTION_DEKU:
                this->dogFlags &= ~DOG_FLAG_JUMP_ATTACKING;
                if ((this->behavior != DOG_BEHAVIOR_DEKU) && (player->actor.speed > 1.0f)) {
                    this->behavior = DOG_BEHAVIOR_DEKU;
                    EnDg_ChangeAnim(&this->skelAnime, sAnimationInfo, DOG_ANIM_RUN);
                    this->actionFunc = EnDg_ApproachPlayerToAttack;
                }

                if ((this->behavior != DOG_BEHAVIOR_DEKU_WAIT) && (this->behavior != DOG_BEHAVIOR_DEKU)) {
                    this->behavior = DOG_BEHAVIOR_DEKU_WAIT;
                    EnDg_SetupIdleMove(this, play);
                }
                break;
        }
    }
    lastOption = recomp_get_config_u32("dog_behavior");
}

RECOMP_PATCH void EnDg_Update(Actor* thisx, PlayState* play) {
    EnDg* this = (EnDg*)thisx;
    Player* player = GET_PLAYER(play);
    s32 pad;
    Vec3f floorRot = { 0.0f, 0.0f, 0.0f };

    this->selectedDogIndex = sSelectedRacetrackDogInfo.index;
    if (!(player->stateFlags1 & PLAYER_STATE1_20) || (play->sceneId != SCENE_CLOCKTOWER)) {
        if (EnDg_ShouldOverrideAction(this, play)) {
            EnDg_ChooseActionForOption(this, play);
        } else if (EnDg_ShouldReactToNonHumanPlayer(this, play)) {
            EnDg_ChooseActionForForm(this, play);
        } else if (this->behavior != DOG_BEHAVIOR_DEFAULT) {
            this->behavior = DOG_BEHAVIOR_DEFAULT;
            EnDg_SetupIdleMove(this, play);
        }

        if ((this->actor.bgCheckFlags & BGCHECKFLAG_WATER_TOUCH) && Actor_HasNoParent(&this->actor, play)) {
            EnDg_ChangeAnim(&this->skelAnime, sAnimationInfo, DOG_ANIM_SWIM);
            this->actionFunc = EnDg_SetupSwim;
        }

        this->actionFunc(this, play);
        EnDg_UpdateCollision(this, play);
        EnDg_GetFloorRot(this, &floorRot);
        Math_ApproachF(&this->curRot.x, floorRot.x, 0.2f, 0.1f);
        Math_ApproachF(&this->curRot.z, floorRot.z, 0.2f, 0.1f);
        SkelAnime_Update(&this->skelAnime);
    }
}

RECOMP_PATCH void EnDg_UpdateCollision(EnDg* this, PlayState* play) {
    Player* player = GET_PLAYER(play);

    this->collider.dim.pos.x = this->actor.world.pos.x;
    this->collider.dim.pos.y = this->actor.world.pos.y;
    this->collider.dim.pos.z = this->actor.world.pos.z;
    Collider_UpdateCylinder(&this->actor, &this->collider);

    // @ThatHypedPerson | I have no idea if this does anything
    if (recomp_get_config_u32("dog_behavior")) {
        if ((recomp_get_config_u32("dog_behavior") == 2) && (this->actionFunc == EnDg_JumpAttack)) {
            CollisionCheck_SetAT(play, &play->colChkCtx, &this->collider.base);
        } else {
            Collider_ResetCylinderAT(play, &this->collider.base);
        }
    } else {
        if ((player->transformation == PLAYER_FORM_DEKU) && (this->actionFunc == EnDg_JumpAttack)) {
            CollisionCheck_SetAT(play, &play->colChkCtx, &this->collider.base);
        } else {
            Collider_ResetCylinderAT(play, &this->collider.base);
        }
    }

    // The check for DOG_FLAG_JUMP_ATTACKING here makes it so the dog passes through the
    // player if it hits them with their jump attack.
    if ((this->grabState != DOG_GRAB_STATE_HELD) && !(this->dogFlags & DOG_FLAG_JUMP_ATTACKING)) {
        CollisionCheck_SetOC(play, &play->colChkCtx, &this->collider.base);
    } else {
        Collider_ResetCylinderOC(play, &this->collider.base);
    }

    Actor_UpdateBgCheckInfo(play, &this->actor, 26.0f, 10.0f, 0.0f, UPDBGCHECKINFO_FLAG_1 | UPDBGCHECKINFO_FLAG_4);
}

RECOMP_HOOK("EnDg_IdleMove")
void EnDg_AllowIdleMovePickup(EnDg* this, PlayState* play) {
    if (recomp_get_config_u32("dog_behavior")) {
        EnDg_TryPickUp(this, play);
    }
}

RECOMP_HOOK("EnDg_IdleBark")
void EnDg_AllowIdleBarkPickup(EnDg* this, PlayState* play) {
    if (recomp_get_config_u32("dog_behavior")) {
        EnDg_TryPickUp(this, play);
    }
}

RECOMP_HOOK("EnDg_ApproachPlayer")
void EnDg_AllowApproachPickup(EnDg* this, PlayState* play) {
    if (recomp_get_config_u32("dog_behavior")) {
        EnDg_TryPickUp(this, play);
    }
}

RECOMP_HOOK("EnDg_SitNextToPlayer")
void EnDg_AllowSitPickup(EnDg* this, PlayState* play) {
    if (recomp_get_config_u32("dog_behavior")) {
        EnDg_TryPickUp(this, play);
    }
}

RECOMP_HOOK("EnDg_BackAwayFromGoron")
void EnDg_AllowBackAwayPickup(EnDg* this, PlayState* play) {
    if (recomp_get_config_u32("dog_behavior")) {
        EnDg_TryPickUp(this, play);
    }
}

RECOMP_HOOK("EnDg_RunAwayFromGoron")
void EnDg_AllowRunningPickup(EnDg* this, PlayState* play) {
    if (recomp_get_config_u32("dog_behavior")) {
        EnDg_TryPickUp(this, play);
    }
}

RECOMP_HOOK("EnDg_ApproachPlayerToAttack")
void EnDg_AllowAttackApproachPickup(EnDg* this, PlayState* play) {
    if (recomp_get_config_u32("dog_behavior")) {
        EnDg_TryPickUp(this, play);
    }
}

RECOMP_HOOK("EnDg_RunAfterAttacking")
void EnDg_AllowRunAfterAttackingPickup(EnDg* this, PlayState* play) {
    if (recomp_get_config_u32("dog_behavior")) {
        EnDg_TryPickUp(this, play);
    }
}

RECOMP_HOOK("EnDg_Thrown")
void EnDg_AllowThrownPickup(EnDg* this, PlayState* play) {
    if (this->actor.bgCheckFlags & BGCHECKFLAG_GROUND && recomp_get_config_u32("dog_behavior")) {
        EnDg_TryPickUp(this, play);
    }
}