#ifndef QFUSION_AI_MANAGER_H
#define QFUSION_AI_MANAGER_H

#include "ai_frame_aware_updatable.h"
#include "ai_goal_entities.h"
#include "static_vector.h"
#include "ai_base_brain.h"

class AiManager : public AiFrameAwareUpdatable
{
	static const unsigned MAX_ACTIONS = AiBaseBrain::MAX_ACTIONS;
	static const unsigned MAX_GOALS = AiBaseBrain::MAX_GOALS;

protected:
	AiManager( const char *gametype, const char *mapname );

	int teams[MAX_CLIENTS];
	ai_handle_t *last;

	int hubAreas[16];
	int numHubAreas;

	static AiManager *instance;
	virtual void Frame() override;

	bool CheckCanSpawnBots();
	void CreateUserInfo( char *buffer, size_t bufferSize );
	edict_t * ConnectFakeClient();
	void SetupClientBot( edict_t *ent );
	void SetupBotTeam( edict_t *ent, const char *teamName );

	// There are no reasons to use a trie. This code might be slow but is called only when a bot enters a game.
	// Tries are not convenient in use for this case and waste heap memory a lot.
	template <typename T, unsigned N>
	class StringValueMap
	{
		StaticVector<std::pair<const char *, T>, N> keyValuePairs;
		unsigned clearLimit;

public:
		inline StringValueMap() : clearLimit( 0 ) {}
		inline ~StringValueMap() {
			clearLimit = 0;
			ClearToLimit();
		}

		T *Get( const char *key );

		// The key must be valid during the entire StringValueMap object lifetime.
		// The key lifetime must be managed by an external (caller) code.
		bool Insert( const char *key, T &&value );

		inline bool IsFull() const { return keyValuePairs.size() == N; }

		inline void MarkClearLimit() { clearLimit = keyValuePairs.size(); }

		inline void ClearToLimit() {
			while( keyValuePairs.size() != clearLimit )
				keyValuePairs.pop_back();
		}

		// For iteration purposes
		std::pair<const char *, T> *begin() { return keyValuePairs.begin(); }
		std::pair<const char *, T> *end() { return keyValuePairs.end(); };
		const std::pair<const char *, T> *begin() const { return keyValuePairs.begin(); }
		const std::pair<const char *, T> *end() const { return keyValuePairs.end(); };
	};

	struct ActionProps {
		char *name;
		// Null if builtin
		void *factoryObject;

		inline ActionProps() {
			// Shut an analyzer up
			memset( this, 0, sizeof( ActionProps ) );
		}

		inline ActionProps( const char *name_, void *factoryObject_ ) {
			auto nameLen = strlen( name_ );
			name = (char *)G_Malloc( nameLen + 1 );
			memcpy( name, name_, nameLen + 1 );
			factoryObject = factoryObject_;
		}
		inline ~ActionProps() {
			if( name ) {
				G_Free( name );
			}
		}
		ActionProps( const ActionProps &that ) = delete;
		ActionProps &operator=( const ActionProps &that ) = delete;

		inline ActionProps( ActionProps &&that ) {
			*this = std::move( that );
		}
		inline ActionProps& operator=( ActionProps &&that ) {
			name = that.name;
			that.name = nullptr;
			factoryObject = that.factoryObject;
			return *this;
		}
	};

	struct GoalProps {
		char *name;
		// Null if builtin
		void *factoryObject;
		unsigned updatePeriod;
		// Can't use StaticVector due to its disabled copy/move ctors
		const char *applicableActions[MAX_ACTIONS];
		unsigned numApplicableActions;

		inline GoalProps() {
			// Shut an analyzer up
			memset( this, 0, sizeof( GoalProps ) );
		}

		inline GoalProps( const char *name_, void *factoryObject_, unsigned updatePeriod_ ) {
			auto nameLen = strlen( name_ );
			name = (char *)G_Malloc( nameLen + 1 );
			memcpy( name, name_, nameLen + 1 );
			factoryObject = factoryObject_;
			updatePeriod = updatePeriod_;
			// Shut an analyzer up
			memset( applicableActions, 0, sizeof( applicableActions ) );
			numApplicableActions = 0;
		}

		GoalProps( const GoalProps &that ) = delete;
		GoalProps &operator=( const GoalProps &that ) = delete;

		inline GoalProps( GoalProps &&that ) {
			*this = std::move( that );
		}
		inline GoalProps& operator=( GoalProps &&that ) {
			name = that.name;
			that.name = nullptr;
			factoryObject = that.factoryObject;
			updatePeriod = that.updatePeriod;
#ifdef _DEBUG
			if( that.numApplicableActions != 0 ) {
				AI_FailWith( "GoalProps(GoalProps &&that)", "Wrong usage pattern. Do not copy non-empty GoalProps\n" );
			}
#endif
			std::copy_n( that.applicableActions, that.numApplicableActions, this->applicableActions );
			numApplicableActions = that.numApplicableActions;
			return *this;
		}

		inline ~GoalProps() {
			if( name ) {
				G_Free( name );
			}
		}
	};

	StringValueMap<ActionProps, MAX_ACTIONS> registeredActions;
	StringValueMap<GoalProps, MAX_GOALS> registeredGoals;

	void RegisterBuiltinGoal( const char *goalName );
	void RegisterBuiltinAction( const char *actionName );

	void SetupBotGoalsAndActions( edict_t *ent );

	void FindHubAreas();
public:
	void LinkAi( ai_handle_t *ai );
	void UnlinkAi( ai_handle_t *ai );

	void OnBotDropped( edict_t *ent );

	static AiManager *Instance() { return instance; }

	static void Init( const char *gametype, const char *mapname );
	static void Shutdown();

	void NavEntityReachedBy( const NavEntity *canceledGoal, const class Ai *goalGrabber );
	void NavEntityReachedSignal( const edict_t *ent );
	void OnBotJoinedTeam( edict_t *ent, int team );

	void RegisterEvent( const edict_t *ent, int event, int parm );

	void SpawnBot( const char *teamName );
	void RespawnBot( edict_t *ent );
	void RemoveBot( const char *name );
	void AfterLevelScriptShutdown();
	void BeforeLevelScriptShutdown();

	void RegisterScriptGoal( const char *goalName, void *factoryObject, unsigned updatePeriod );
	void RegisterScriptAction( const char *actionName, void *factoryObject );
	void AddApplicableAction( const char *goalName, const char *actionName );

	inline void UnregisterScriptGoalsAndActions() {
		// There should not be any bots connected when this method gets called
		registeredGoals.ClearToLimit();
		registeredActions.ClearToLimit();
	}

	bool IsAreaReachableFromHubAreas( int targetArea, float *score = 0 ) const;
};

#endif
