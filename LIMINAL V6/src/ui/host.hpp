// Schnittstelle der Oberflaeche zur Anwendung. Die Menues rufen nur diese
// Funktionen auf und kennen die Anwendung selbst nicht.
#pragma once

#include <string>

namespace lim {
struct Settings;
namespace game {
class Content;
class GameSession;
struct SaveEntry;
}  // namespace game
namespace gfx {
class Renderer;
}
namespace input {
class Input;
}
}  // namespace lim

namespace lim::ui {

class Host {
public:
    virtual ~Host() = default;
    virtual Settings& settings() = 0;
    virtual void settingsChanged() = 0;  // anwenden und speichern
    virtual const game::Content& content() = 0;
    virtual game::GameSession* session() = 0;  // nur waehrend des Spiels
    virtual gfx::Renderer& renderer() = 0;
    virtual input::Input& input() = 0;
    virtual void newGame(const std::string& name, const std::string& difficulty, const std::string& level) = 0;
    virtual bool loadGame(const game::SaveEntry& e) = 0;
    virtual void saveGame() = 0;
    virtual void toTitle() = 0;
    virtual void quit() = 0;
    virtual void playSound(const std::string& name, float volume = 1.0f) = 0;
    virtual double time() const = 0;
    virtual bool hasSave() const = 0;
    virtual void loadLastSave() = 0;
    virtual float fps() const = 0;
    virtual std::string debugInfo() = 0;
};

}  // namespace lim::ui
