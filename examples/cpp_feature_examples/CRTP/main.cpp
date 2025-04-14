/*
 * @author: Avidel
 * @LastEditors: Avidel
 */
#include "spdlog/spdlog.h"
class Character {
    public:
        virtual void getName(){
            spdlog::info("Character");
        };
};

class Hero: public Character {
    public:
        void getName() override {
            spdlog::info("Hero");
        }
};

class Villain: public Character {
    public:
        void getName() override {
            spdlog::info("Villain");
        }
};

void getCharacterName(Character* character) {
    character->getName();
}

template<typename Derived>
class CharacterTemplate{
    public:
        void getName(){
            static_cast<Derived*>(this)->getName();
        }
};

class HeroTemplate: public CharacterTemplate<HeroTemplate>{
    public:
        void getName() {
            spdlog::info("HeroTemplate");
        }
};

template<typename Derived>
void getCharacterNameTemplate(CharacterTemplate<Derived> character) {
    character.getName();
}


int main(){
    Hero* hero = new Hero();
    Villain villain;
    getCharacterName(hero);

    spdlog::info("----Using CRTP----");
    HeroTemplate heroTemplate;
    getCharacterNameTemplate(heroTemplate);
    return 0;
}