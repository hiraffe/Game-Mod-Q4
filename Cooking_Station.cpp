#include "../idlib/precompiled.h"
#pragma hdrstop

#include "Game_local.h"
#include "Cooking_Station.h"

CLASS_DECLARATION( idAnimatedEntity, rvCookingStation )
END_CLASS

/*
================
rvCookingStation::Think
================
*/
void rvCookingStation::Think(void) {
	// TODO: I'm guessing this is bad, but I wanted to get this in so that people could start 
	// placing it.  The entity decided to stop thinking and I didn't have time to debug it.
	BecomeActive(TH_ALL);

	stateThread.Execute();
	UpdateAnimation();

	if (thinkFlags & TH_UPDATEVISUALS) {
		if (healthDispensed > 0) {
			CreateFrame(float(healthDispensed) / maxHealth);
		}
		Present();
	}
}

/*
================
rvCookingStation::Spawn
================
*/
void rvCookingStation::Spawn(void) {
	entityToHeal = 0;
	nextHealTime = 0;
	healFrequency = spawnArgs.GetInt("heal_frequency", "24");
	healAmount = spawnArgs.GetInt("heal_amount", "1");

	healthDispensed = 0;
	soundStartTime = 0;
	soundLength = 0;
	maxHealth = spawnArgs.GetInt("max_health", "100");

	dispenseAnim = GetAnimator()->GetAnim(spawnArgs.GetString("dispense_anim", "dispense"));

	CreateFrame(0);

	stateThread.SetOwner(this);
	stateThread.SetName(GetName());
	GetAnimator()->CycleAnim(ANIMCHANNEL_ALL, GetAnimator()->GetAnim(spawnArgs.GetString("anim", "idle")), gameLocal.time, 4);
}

/*
================
rvCookingStation::Save
================
*/
void rvCookingStation::Save(idSaveGame* savefile) const {
	stateThread.Save(savefile);
	entityToHeal.Save(savefile);
	savefile->WriteInt(nextHealTime);
	savefile->WriteInt(healFrequency);
	savefile->WriteInt(healAmount);
	savefile->WriteInt(healthDispensed);
	savefile->WriteInt(maxHealth);
	savefile->WriteInt(dispenseAnim);
	savefile->WriteInt(soundStartTime);
	savefile->WriteInt(soundLength);
}

/*
================
rvCookingStation::Restore
================
*/
void rvCookingStation::Restore(idRestoreGame* savefile) {
	stateThread.Restore(savefile, this);
	entityToHeal.Restore(savefile);
	savefile->ReadInt(nextHealTime);
	savefile->ReadInt(healFrequency);
	savefile->ReadInt(healAmount);
	savefile->ReadInt(healthDispensed);
	savefile->ReadInt(maxHealth);
	savefile->ReadInt(dispenseAnim);
	savefile->ReadInt(soundStartTime);
	savefile->ReadInt(soundLength);
}

/*
================
rvCookingStation::BeginCooking
================
*/
void rvCookingStation::BeginCooking(idEntity* toHeal) {
	entityToHeal = toHeal;
	stateThread.SetState("Cooking");
}

/*
================
rvCookingStation::EndCooking
================
*/
void rvCookingStation::EndCooking(void) {
	entityToHeal = NULL;
}

/*
================
rvCookingStation::CreateFrame
================
*/
void rvCookingStation::CreateFrame(float station_health) {
	// Update the GUI
	if (renderEntity.gui[0]) {
		renderEntity.gui[0]->SetStateFloat("station_health", 1.0f - station_health);
		renderEntity.gui[0]->StateChanged(gameLocal.time, true);
	}

	// Update the Animation
	int numFrames = GetAnimator()->GetAnim(dispenseAnim)->NumFrames();
	float lerp = numFrames * station_health;
	int frame = lerp;
	lerp = lerp - frame;
	frameBlend_t frameBlend = { 0, frame, frame + 1, 1.0f - lerp, lerp };
	GetAnimator()->SetFrame(ANIMCHANNEL_ALL, dispenseAnim, frameBlend);
}

/*
================
rvCookingStation::IsPlaying
================
*/
bool rvCookingStation::IsPlaying(void) {
	idSoundEmitter* emitter = soundSystem->EmitterForIndex(SOUNDWORLD_GAME, GetSoundEmitter());
	if (emitter) {
		return (emitter->CurrentlyPlaying());
	}
	return false;
}

/*
===============================================================================

	States

===============================================================================
*/

CLASS_STATES_DECLARATION(rvCookingStation)
STATE("Cooking", rvCookingStation::State_Cooking)
END_CLASS_STATES

/*
================
rvCookingStation::State_Cooking
================
*/
stateResult_t rvCookingStation::State_Cooking(const stateParms_t& parms) {
	enum {
		STAGE_INIT,
		STAGE_WAIT,
		STAGE_DISPENSE,
	};

	//COOKING MINIGAME HERE YAY
	if (entityToHeal.IsValid()) {
		idPlayer* player = static_cast<idPlayer*>(entityToHeal.GetEntity());
		const int entityMaxHealth = player->inventory.maxHealth;

		if (healthDispensed < maxHealth &&			// and we have health to dispense...
			entityToHeal->health < entityMaxHealth &&	// and the entity needs health.
			entityToHeal->health   > 0)					// and he's still alive.
		{
			switch (parms.stage) {
			case STAGE_INIT:
				soundStartTime = gameLocal.time;
				StartSound("snd_start", SND_CHANNEL_ANY, 0, false, &soundLength);
				return SRESULT_STAGE(STAGE_WAIT);

			case STAGE_WAIT:
				if (gameLocal.time > soundStartTime + soundLength) {
					soundStartTime = 0;
					soundLength = 0;
					return SRESULT_STAGE(STAGE_DISPENSE);
				}
				return SRESULT_WAIT;

			case STAGE_DISPENSE:
				if (gameLocal.time > nextHealTime) {	// If it's time to heal...
					int healthGiven = Min(maxHealth - healthDispensed, Min(healAmount, entityMaxHealth - entityToHeal->health));
					entityToHeal->health += healthGiven;
					healthDispensed += healthGiven;
					nextHealTime = gameLocal.time + healFrequency;
				}
				if (!IsPlaying()) {
					StartSound("snd_loop", SND_CHANNEL_ANY, 0, false, NULL);
				}
				return SRESULT_WAIT;
			}
		}
	}

	StopSound(SND_CHANNEL_ANY, 0);
	StartSound("snd_stop", SND_CHANNEL_ANY, 0, false, NULL);
	return SRESULT_DONE;
}
