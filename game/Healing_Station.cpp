#include "../idlib/precompiled.h"
#pragma hdrstop

#include "Game_local.h"
#include "Healing_Station.h"

CLASS_DECLARATION( idAnimatedEntity, rvHealingStation )
END_CLASS

/*
================
rvHealingStation::Think
================
*/
void rvHealingStation::Think ( void ) {
	// TODO: I'm guessing this is bad, but I wanted to get this in so that people could start 
	// placing it.  The entity decided to stop thinking and I didn't have time to debug it.
	BecomeActive( TH_ALL );

	stateThread.Execute();
	UpdateAnimation();

	if ( thinkFlags & TH_UPDATEVISUALS ) {
		if ( healthDispensed > 0 ) {
			CreateFrame( float( healthDispensed ) / maxHealth );
		}
		Present();
	}
	
}

/*
================
rvHealingStation::Spawn
================
*/
void rvHealingStation::Spawn ( void ) {
	entityToHeal	= 0;
	nextHealTime	= 0;
	healFrequency	= spawnArgs.GetInt( "heal_frequency", "24" );
	healAmount		= spawnArgs.GetInt( "heal_amount", "1" );
	
	healthDispensed	= 0;
	soundStartTime	= 0;
	soundLength		= 0;
	maxHealth		= spawnArgs.GetInt( "max_health", "100" );

	dispenseAnim	= GetAnimator()->GetAnim( spawnArgs.GetString( "dispense_anim", "dispense" ) );

	CreateFrame( 0 );

	stateThread.SetOwner( this );
	stateThread.SetName( GetName() );
	GetAnimator()->CycleAnim( ANIMCHANNEL_ALL, GetAnimator()->GetAnim( spawnArgs.GetString( "anim", "idle" ) ), gameLocal.time, 4 );
}

/*
================
rvHealingStation::Save
================
*/
void rvHealingStation::Save ( idSaveGame *savefile ) const {
	stateThread.Save( savefile );
	entityToHeal.Save ( savefile );
	savefile->WriteInt( nextHealTime );
	savefile->WriteInt( healFrequency );
	savefile->WriteInt( healAmount );
	savefile->WriteInt( healthDispensed );
	savefile->WriteInt( maxHealth );
	savefile->WriteInt( dispenseAnim );
	savefile->WriteInt( soundStartTime );
	savefile->WriteInt( soundLength );
}

/*
================
rvHealingStation::Restore
================
*/
void rvHealingStation::Restore ( idRestoreGame *savefile ) {
	stateThread.Restore( savefile, this );
	entityToHeal.Restore ( savefile );
	savefile->ReadInt( nextHealTime );
	savefile->ReadInt( healFrequency );
	savefile->ReadInt( healAmount );
	savefile->ReadInt( healthDispensed );
	savefile->ReadInt( maxHealth );
	savefile->ReadInt( dispenseAnim );
	savefile->ReadInt( soundStartTime );
	savefile->ReadInt( soundLength );
}

/*
================
rvHealingStation::BeginHealing
================
*/
void rvHealingStation::BeginHealing ( idEntity *toHeal ) {
	entityToHeal	= toHeal;
	stateThread.SetState( "Cooking" );

	//loop of monsters that need to be fed
	/*const char* hungry[5];
	idEntity* newEnt = NULL;
	//for (int i = 0; i < 5; i++) {
		//int recipe = rand() % (5) + 1;
		idDict		dict;
		idPlayer* player = static_cast<idPlayer*>(entityToHeal.GetEntity());
		int yaw = player->viewAngles.yaw;
		dict.Set("angle", va("%f", yaw + 180));
		idVec3 org = player->GetPhysics()->GetOrigin() + idAngles(0, yaw, 0).ToForward() * 80 + idVec3(0, 0, 1);
		dict.Set("origin", org.ToString());

		//if (recipe == 1) {
		dict.Set("classname", "monster_grunt");	//"char_marine_npc_voss_airdefense");

			gameLocal.SpawnEntityDef(dict, &newEnt);
			//hungry[0] = newEnt->name.c_str();
		//}
		if (newEnt) {
			gameLocal.Printf("spawned entity '%s'\n", newEnt->name.c_str());
		}
	//}
	//end*/
}

/*
================
rvHealingStation::EndHealing
================
*/
void rvHealingStation::EndHealing ( void ) {
	entityToHeal	= NULL;
}

/*
================
rvHealingStation::CreateFrame
================
*/
void rvHealingStation::CreateFrame ( float station_health ) {
	// Update the GUI
	if ( renderEntity.gui[ 0 ] ) {
		renderEntity.gui[ 0 ]->SetStateFloat( "station_health", 1.0f - station_health );
		renderEntity.gui[ 0 ]->StateChanged( gameLocal.time, true );
	}

	// Update the Animation
	int numFrames	= GetAnimator()->GetAnim( dispenseAnim )->NumFrames();
	float lerp		= numFrames * station_health;
	int frame		= lerp;
	lerp			= lerp - frame;
	frameBlend_t frameBlend	= { 0, frame, frame + 1, 1.0f - lerp, lerp };
	GetAnimator()->SetFrame( ANIMCHANNEL_ALL, dispenseAnim, frameBlend );	
}

/*
================
rvHealingStation::IsPlaying
================
*/
bool rvHealingStation::IsPlaying ( void ) {
	idSoundEmitter* emitter = soundSystem->EmitterForIndex ( SOUNDWORLD_GAME, GetSoundEmitter ( ) );
	if( emitter ) {
		return ( emitter->CurrentlyPlaying ( ) );
	}
	return false;
}

/*
===============================================================================

	States 

===============================================================================
*/

CLASS_STATES_DECLARATION ( rvHealingStation )
	STATE ( "Cooking",		rvHealingStation::State_Healing )
END_CLASS_STATES

/*
================
rvHealingStation::State_Healing which is actually state COOKING
================
*/
stateResult_t rvHealingStation::State_Healing ( const stateParms_t& parms ) {
	enum { 
		STAGE_INIT,
		STAGE_WAIT,
		STAGE_DISPENSE,
	};

	//check if player has the proper ingredients, start cooking
	if ( entityToHeal.IsValid() ) {
		idPlayer* player = static_cast<idPlayer*>( entityToHeal.GetEntity( ) );
		maxHealth = 50;
		
		if ( healthDispensed < maxHealth )		// and we have health to dispense...
		{
			switch ( parms.stage ) {
				case STAGE_INIT:
					soundStartTime = gameLocal.time;
					StartSound( "snd_start", SND_CHANNEL_ANY, 0, false, &soundLength );
					return SRESULT_STAGE ( STAGE_WAIT );

				case STAGE_WAIT:
					if ( gameLocal.time > soundStartTime + soundLength ) {
						soundStartTime = 0;
						soundLength = 0;
						return SRESULT_STAGE ( STAGE_DISPENSE );
					}
					return SRESULT_WAIT;

				case STAGE_DISPENSE:

					if ( gameLocal.time			> nextHealTime ) {	// If it's time to heal...
						int healthGiven = Min(maxHealth - healthDispensed, Min(healAmount, 50));
						healthDispensed			+= healthGiven;
						nextHealTime			= gameLocal.time + healFrequency;
					}
					if ( !IsPlaying ( ) ) {
						StartSound( "snd_loop", SND_CHANNEL_ANY, 0, false, NULL );
					}
					return SRESULT_WAIT;
			}
		}
		//me
		//gameLocal.Printf("Weapons: %d", player->inventory.weapons);
		if (player->inventory.weapons == 1049) { //cooking
			gameLocal.Printf("Pizza Cooked\n");
			idEntity* ent = gameLocal.FindEntity(""); //find the exact name of the monster spawned
			if (!ent) {
				gameLocal.Printf("Nobody asked for a pizza.\n");
			}
			else {
				delete ent;
			}

		}
		else if (player->inventory.weapons == 1039) { //grilling
			gameLocal.Printf("Burger Grilled\n");
		}
		else if (player->inventory.weapons == 737) { //baking
			gameLocal.Printf("Cake Baked\n");
		}
		else if (player->inventory.weapons == 275) { //chopping
			gameLocal.Printf("Salad Tossed\n");
		}
		else if (player->inventory.weapons == 769) { //blending
			gameLocal.Printf("Smoothie Blended\n");
		}
		else {
			gameLocal.Printf("None\n");
		}
		player->inventory.weapons = 1;
		//me end
	}

	StopSound ( SND_CHANNEL_ANY, 0 );
	StartSound ( "snd_stop", SND_CHANNEL_ANY, 0, false, NULL );
	Spawn();
	//return SRESULT_DONE;
}
