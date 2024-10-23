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
	
	player = static_cast<idPlayer*>(entityToHeal.GetEntity());

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
	idEntity* newEnt = NULL;
	int recipe = rand() % 5;
	gameLocal.Printf("recipe number: %d\n", recipe);

	switch (recipe) {
	case 0:
		dict.Set("angle", "1693882920");
		dict.Set("origin", "10452.56 -2776.22 1.25");
		dict.Set("classname", "char_marine_npc_voss_airdefense");
		break;
	case 1:
		dict.Set("angle", "1693964840");
		dict.Set("origin", "10418.2 -2745.65 1.25");
		dict.Set("classname", "char_marine_npc_sledge_airdefense");
		break;
	case 2:
		dict.Set("angle", "1693899304");
		dict.Set("origin", "10455.62 -2655.08 1.25");
		dict.Set("classname", "char_marine_npc_cortez_airdefense");
		break;
	case 3:
		dict.Set("angle", "1693936168");
		dict.Set("origin", "10444.95 -2718.97 1.25");
		dict.Set("classname", "char_marine_npc_bidwell_airdefense");
		break;
	case 4:
		dict.Set("angle", "1693866536");
		dict.Set("origin", "10419.44 -2692.75 1.25");
		dict.Set("classname", "char_marine_npc_morris_airdefense");
		break;
	default: 
		gameLocal.Printf("Wtf going on");
	}
	
	gameLocal.SpawnEntityDef(dict, &newEnt);
	hungry[recipe] = newEnt->name.c_str();
	if (newEnt) {
		gameLocal.Printf("spawned entity '%s'\n", hungry[recipe]);
	}
	//end
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
		idEntity* newEnt = NULL;
		if (player->inventory.weapons == 1049) { //cooking
			gameLocal.Printf("Pizza Cooked\n");
			dict.Set("angle", "1695001128");
			dict.Set("origin", "10664.11 -3138.3 1.25");
			dict.Set("classname", "weapon_grenadelauncher"); //tomato
			gameLocal.SpawnEntityDef(dict, &newEnt);
			gameLocal.Printf("Pizza Cooked\n");
			dict.Set("angle", "1694575144");
			dict.Set("origin", "10615.03 -3143.7 1.25");
			dict.Set("classname", "weapon_hyperblaster"); //bread
			gameLocal.SpawnEntityDef(dict, &newEnt);
			dict.Set("angle", "1694624296");
			dict.Set("origin", "10560.41 -3140.67 1.25");
			dict.Set("classname", "weapon_napalmgun"); //cheese
			gameLocal.SpawnEntityDef(dict, &newEnt);
		}
		else if (player->inventory.weapons == 1039) { //grilling
			gameLocal.Printf("Burger Grilled\n");
			dict.Set("angle", "1694575144");
			dict.Set("origin", "10615.03 -3143.7 1.25");
			dict.Set("classname", "weapon_hyperblaster"); //bread
			gameLocal.SpawnEntityDef(dict, &newEnt);
			dict.Set("angle", "1685940776");
			dict.Set("origin", "10511.74 -3134.48 1.25");
			dict.Set("classname", "weapon_machinegun"); //lettuce
			gameLocal.SpawnEntityDef(dict, &newEnt);
			dict.Set("angle", "1694624296");
			dict.Set("origin", "10560.41 -3140.67 1.25");
			dict.Set("classname", "weapon_napalmgun"); //cheese
			gameLocal.SpawnEntityDef(dict, &newEnt);
			dict.Set("angle", "169462496");
			dict.Set("origin", "10469.22 -3005.57 1.25");
			dict.Set("classname", "weapon_shotgun"); //beef
			gameLocal.SpawnEntityDef(dict, &newEnt);
		}
		else if (player->inventory.weapons == 737) { //baking
			gameLocal.Printf("Cake Baked\n");
			dict.Set("angle", "1685908008");
			dict.Set("origin", "10467.6 -3141.33 1.25");
			dict.Set("classname", "weapon_nailgun"); //flour
			gameLocal.SpawnEntityDef(dict, &newEnt);
			dict.Set("angle", "1685957160");
			dict.Set("origin", "10418.51 -3139.44 1.25");
			dict.Set("classname", "weapon_railgun"); //sugar
			gameLocal.SpawnEntityDef(dict, &newEnt);
			dict.Set("angle", "1694525992");
			dict.Set("origin", "10370.97 -3142.44 1.25");
			dict.Set("classname", "weapon_rocketlauncher"); //eggs
			gameLocal.SpawnEntityDef(dict, &newEnt);
			dict.Set("angle", "1694558760");
			dict.Set("origin", "10372.06 -3011.14 1.25");
			dict.Set("classname", "weapon_dmg"); //milk
			gameLocal.SpawnEntityDef(dict, &newEnt);
		}
		else if (player->inventory.weapons == 275) { //chopping
			gameLocal.Printf("Salad Tossed\n");
			dict.Set("angle", "1695001128");
			dict.Set("origin", "10664.11 -3138.3 1.25");
			dict.Set("classname", "weapon_grenadelauncher"); //tomato
			gameLocal.SpawnEntityDef(dict, &newEnt);
			dict.Set("angle", "1685940776");
			dict.Set("origin", "10511.74 -3134.48 1.25");
			dict.Set("classname", "weapon_machinegun"); //lettuce
			gameLocal.SpawnEntityDef(dict, &newEnt);
			dict.Set("angle", "1694509608");
			dict.Set("origin", "10420.27 -3009.3 1.25");
			dict.Set("classname", "weapon_lightninggun"); //berries
			gameLocal.SpawnEntityDef(dict, &newEnt);
		}
		else if (player->inventory.weapons == 769) { //blending
			gameLocal.Printf("Smoothie Blended\n");
			dict.Set("angle", "1694558760");
			dict.Set("origin", "10372.06 -3011.14 1.25");
			dict.Set("classname", "weapon_dmg"); //milk
			gameLocal.SpawnEntityDef(dict, &newEnt);
			dict.Set("angle", "1694509608");
			dict.Set("origin", "10420.27 -3009.3 1.25");
			dict.Set("classname", "weapon_lightninggun"); //berries
			gameLocal.SpawnEntityDef(dict, &newEnt);
			/*
			if (hungry[4] != NULL) {
				gameLocal.Printf("Hungry guy: %s\n", hungry[4]);
				idEntity* ent = gameLocal.FindEntity(hungry[4]); //find the exact name of the hungry guy
				gameLocal.Printf("%s removed.\n", hungry[4]);
				if (!ent) {
					gameLocal.Printf("Nobody asked for a smoothie.\n");
				}
				else {
					delete ent;
				}
			}
			*/
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
