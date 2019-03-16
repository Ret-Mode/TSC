/***************************************************************************
 * scene.cpp -- scene handling class
 *
 * Copyright © 2003 - 2011 Florian Richter
 * Copyright © 2012-2019 The TSC Contributors
 ***************************************************************************/
/*
   This program is free software; you can redistribute it and/or modify
   it under the terms of the GNU General Public License as published by
   the Free Software Foundation; either version 3 of the License, or
   (at your option) any later version.

   You should have received a copy of the GNU General Public License
   along with this program.  If not, see <http://www.gnu.org/licenses/>.
*/

#include "scene.hpp"
#include "../core/sprite_manager.hpp"
#include "../core/game_core.hpp"
#include "../core/filesystem/filesystem.hpp"
#include "../input/mouse.hpp"
#include "../input/keyboard.hpp"
#include "../input/joystick.hpp"
#include "../audio/audio.hpp"
#include "../gui/hud.hpp"
#include "../gui/game_console.hpp"
#include "../level/level.hpp"
#include "scene_actions.hpp"
#include "01_prologue.hpp"

namespace fs = boost::filesystem;
using namespace std;
using namespace TSC;

cScene::cScene(void)
    : mp_sprite_manager(new cSprite_Manager()),
      mp_camera(new cCamera(mp_sprite_manager)),
      mp_scene_image(new cSprite(mp_sprite_manager)),
      mp_story_box(nullptr)
{
    mp_scene_image->Set_Massive_Type(MASS_PASSIVE);
    mp_scene_image->Set_Active(true);
    mp_scene_image->Set_Spawned(false);
    mp_scene_image->Set_Scale_Affects_Rect(true);
    mp_sprite_manager->Add(mp_scene_image);

    /* Construct the story narration box. The box is attached in
     * Enter(), detached in Leave(), and destroyed in the destructor.
     * This allows having multiple cScene instances around without
     * CEGUI complaining about colliding window names.
     * It is shown when Show_Story_Box() is called during the
     * action sequence execution. */
    CEGUI::WindowManager& wmgr = CEGUI::WindowManager::getSingleton();
    mp_story_box = wmgr.createWindow("TSCLook256/StaticText", "story_box");
    mp_story_box->setPosition(CEGUI::UVector2(CEGUI::UDim(0.2f, 0), CEGUI::UDim(0.7f, 0)));
    mp_story_box->setSize(CEGUI::USize(CEGUI::UDim(0.6f, 0), CEGUI::UDim(0.2f, 0)));
    mp_story_box->setAlpha(0.8f);
    mp_story_box->setTextParsingEnabled(true);
    mp_story_box->setProperty("HorzFormatting", "WordWrapLeftAligned"); // You'll never find the CEGUI property documentation if you don't know where it is. It's here: http://static.cegui.org.uk/static/TaharezLookProperties.html#HorzFormatting
    mp_story_box->hide();
}

cScene::~cScene(void)
{
    // Already detached in Leave(). If not, CEGUI's destroyWindow()
    // probably takes care of detaching the window from its parent.
    CEGUI::WindowManager::getSingleton().destroyWindow(mp_story_box);

    delete mp_camera;
    delete mp_sprite_manager; // Delete mp_scene_image
}

void cScene::Enter(const GameMode old_mode)
{
    pMouseCursor->Set_Sprite_Manager(mp_sprite_manager);
    pMouseCursor->Set_Active(false);
    gp_hud->Hide();
    gp_game_console->Hide();

    pActive_Camera = mp_camera;

    mp_scene_image->Set_Pos(0, -game_res_h, true);

    // Attach narration box
    CEGUI::System::getSingleton().
        getDefaultGUIContext().
        getRootWindow()->
        addChild(mp_story_box);
}

void cScene::Leave(const GameMode next_mode)
{
    pMouseCursor->Reset(false);
    pAudio->Fadeout_Music(1000);
    pJoystick->Reset_keys();

    // Detach story narration box
    CEGUI::System::getSingleton().
        getDefaultGUIContext().
        getRootWindow()->
        removeChild(mp_story_box);
}

void cScene::Update(void)
{
    // Execute all actions in the action sequence
    for (auto iter=m_action_sequence.begin(); iter != m_action_sequence.end(); ) {
        if ((*iter)->Execute()) {
            // Remove action when it signals it has finished.
            delete *iter;
            iter = m_action_sequence.erase(iter);
        }
        else {
            // Let it at start, thereby making it available to the input handler functions.
            break;
        }
    }

    pActive_Camera->Update();

    // End the scene if the action sequence is empty, and return
    // to the active level.
    // (Changing Game_Action subsequently causes the main loop
    // to call cScene::Leave() and then destroy it).
    if (m_action_sequence.size() == 0) {
        Game_Action = GA_ENTER_LEVEL;
        Game_Action_Data_Start.add("screen_fadeout", int_to_string(EFFECT_OUT_HORIZONTAL_VERTICAL));
        Game_Action_Data_Start.add("screen_fadeout_speed", "3");

        if (m_next_level.empty()) { // Resume pActive_level
            if (!pActive_Level) { // This is an error by the scene author!
                throw(std::runtime_error("Bug: Requested resume of pActive_Level, but it is NULL"));
            }

            if (!m_next_level_entry.empty()) { // Switch to level entry in pActive_Level
                Game_Action_Data_Middle.add("activate_level_entry", m_next_level_entry);
            }
        }
        else { // Switch to given level
            Game_Action_Data_Middle.add("load_level", m_next_level);
            if (!m_next_level_entry.empty()) { // Switch to level entry in target level
                Game_Action_Data_Middle.add("load_level_entry", m_next_level_entry);
                // Note: load_level shouldn't be combined with activate_level_entry,
                // as there's load_level_entry for it, which is more specific.
            }
        }

        Game_Action_Data_End.add("screen_fadein", int_to_string(EFFECT_IN_BLACK));
        Game_Action_Data_End.add("screen_fadein_speed", "3");
    }
}

void cScene::Draw(void)
{
    // clear
    pVideo->Clear_Screen();
    // Draw scene
    mp_sprite_manager->Draw_Items();
}

void cScene::Set_Next_Level(std::string level, std::string entry)
{
    m_next_level = level;
    m_next_level_entry = entry;
}

void cScene::Set_Scene_Image(std::string scene_image)
{
    mp_scene_image->Set_Image(pVideo->Get_Surface(utf8_to_path(scene_image)), true);
}

void cScene::Show_Story_Box()
{
    mp_story_box->show();
}

void cScene::Hide_Story_Box()
{
    mp_story_box->hide();
}

void cScene::Set_Story_Text(std::string text)
{
    mp_story_box->setText(reinterpret_cast<const CEGUI::utf8*>(text.c_str()));
}

bool cScene::Key_Down(const sf::Event& evt)
{
    return m_action_sequence[0]->Key_Down(evt);
}

bool cScene::Key_Up(const sf::Event& evt)
{
    return m_action_sequence[0]->Key_Up(evt);
}

bool cScene::Mouse_Down(sf::Mouse::Button button)
{
    return m_action_sequence[0]->Mouse_Down(button);
}

bool cScene::Mouse_Up(sf::Mouse::Button button)
{
    return m_action_sequence[0]->Mouse_Up(button);
}

bool cScene::Joy_Button_Down(unsigned int button)
{
    return m_action_sequence[0]->Joy_Button_Down(button);
}

bool cScene::Joy_Button_Up(unsigned int button)
{
    return m_action_sequence[0]->Joy_Button_Up(button);
}

cScene* cScene::Load_Scene(enum Scenes scene)
{
    switch (scene) {
    case SCENE_PROLOGUE:
        return new cPrologueScene();
    default:
        throw(std::runtime_error("Unknown scene requested; terminating."));
    }
}

// Currently running scene, NULL if no scene is active
cScene* TSC::pActive_Scene = nullptr;
