#include "narrative_language.h"
#include <array>
#include <sstream>
#include <unordered_set>
#include <regex>

namespace liminal {
namespace { std::unordered_set<std::string> Words(std::string text); }
std::string ConstrainArrivalDirections(const std::string& text,GameLanguage language,
    const std::map<std::string,bool>& exits,CardinalDirection return_direction,bool append_routes) {
    // French "est" is also a verb: only treat it as east in an orientation phrase.
    static const auto compass=Words("north east south west nord sud ouest nordost nordwest sudost sudwest nordouest sudouest nordest sudest nordøst sydøst nordvest sydvest norden osten süden westen sued ost ovest syd sør sor øst vest");
    static const std::regex french_east(R"((\b(vers|au|de|du|depuis|par|retour|direction|passage|sortie)|à|À)\s*(:\s*)?(l['’ ]*)?[Ee][Ss][Tt]\b|\b[Ee][Ss][Tt]\s*[:;,])",std::regex::icase);
    static const char* names[6][4]={{"north","east","south","west"},{"nord","est","sud","ouest"},
        {"nord","øst","sør","vest"},{"nord","øst","syd","vest"},{"Norden","Osten","Süden","Westen"},{"nord","est","sud","ovest"}};
    static const char* headings[6]={"Open passages: ","Passages ouverts : ","Åpne passasjer: ","Åbne passager: ","Offene Wege: ","Passaggi aperti: "};
    static const char* backs[6]={"Way back: ","Retour : ","Tilbake: ","Tilbage: ","Rückweg: ","Ritorno: "};
    const int lang=language>=kGameLanguageEnglish&&language<=kGameLanguageItalian?static_cast<int>(language):0;
    std::string result;
    size_t begin=0;
    while(begin<text.size()) {
        const auto end=text.find_first_of(".!?\n",begin);
        auto sentence=text.substr(begin,(end==std::string::npos?text.size():end+1)-begin);
        bool names_compass=false;
        for(const auto& word:Words(sentence))
            if(compass.count(word)||(language==kGameLanguageItalian&&word=="est"))names_compass=true;
        if(!names_compass&&!std::regex_search(sentence,french_east)) {
            const auto first=sentence.find_first_not_of(" \t\r\n");
            if(first!=std::string::npos&&sentence.compare(first,std::char_traits<char>::length(headings[lang]),headings[lang])!=0&&sentence.compare(first,std::char_traits<char>::length(backs[lang]),backs[lang])!=0){if(!result.empty())result+=' ';result+=sentence.substr(first);}
        }
        if(end==std::string::npos)break;begin=end+1;
    }
    if(!append_routes)return result;
    const char* keys[]={"north","east","south","west"};std::string routes,back;
    const CardinalDirection directions[]={kDirectionNorth,kDirectionEast,kDirectionSouth,kDirectionWest};
    for(int i=0;i<4;++i) {
        const auto exit=exits.find(keys[i]);
        if(exit!=exits.end()&&exit->second){if(!routes.empty())routes+=", ";routes+=names[lang][i];
            if(return_direction==directions[i])back=names[lang][i];}
    }
    if(!routes.empty()){if(!result.empty())result+=' ';result+=std::string(headings[lang])+routes+".";}
    if(!back.empty()){if(!result.empty())result+=' ';result+=std::string(backs[lang])+back+".";}
    return result;
}
namespace {
size_t LanguageIndex(GameLanguage language) {
    return language >= kGameLanguageEnglish && language <= kGameLanguageItalian ?
        static_cast<size_t>(language) : 0;
}
std::unordered_set<std::string> Words(std::string text) {
    // ASCII and Latin-1 uppercase folding, preserving UTF-8 word boundaries.
    for(size_t i=0;i<text.size();++i) {
        auto c=static_cast<unsigned char>(text[i]);
        if(c==0xc3 && i+1<text.size()) {
            auto next=static_cast<unsigned char>(text[i+1]);
            if(next>=0x80 && next<=0x9e && next!=0x97)text[i+1]=static_cast<char>(next+0x20);
            ++i;
        } else if(c>='A'&&c<='Z')text[i]=static_cast<char>(c+32);
        else if(c<128 && !(c>='a'&&c<='z'))text[i]=' ';
        else if(c==0xe2 && i+2<text.size() && static_cast<unsigned char>(text[i+1])==0x80) {
            text[i]=text[i+1]=text[i+2]=' '; i+=2; // Curly quotes/dashes.
        }
    }
    std::istringstream input(text);std::unordered_set<std::string> words;
    for(std::string word;input>>word;)words.insert(word);
    return words;
}
}

std::string NarrativeLanguageInstruction(GameLanguage language) {
    static const char* instructions[]={
        "Write exclusively in English. Address the explorer as you, in the present tense.",
        "Écrivez exclusivement en français. Adressez-vous à l'explorateur au présent avec vous, jamais tu.",
        "Skriv bare på norsk bokmål. Henvend deg til utforskeren med du og skriv i presens.",
        "Skriv udelukkende på dansk. Henvend dig til den opdagelsesrejsende med du og skriv i nutid.",
        "Schreibe ausschließlich auf Deutsch. Sprich die erkundende Person mit du an und schreibe im Präsens.",
        "Scrivi esclusivamente in italiano. Rivolgiti all'esploratore con tu, al presente, mai con io o Lei."
    };
    static const char* names[]={"English","French","Norwegian Bokmal","Danish","German","Italian"};
    const auto index=LanguageIndex(language);
    return std::string(instructions[index])+" OUTPUT LANGUAGE: "+names[index]+
        ". This language was selected at the start of the game and is binding on EVERY turn, including refusals, revisits and questions. "
        "Write title, summary, narration and thermal.reason ONLY in this language. Keep JSON keys and direction IDs unchanged. "
        "The language of commands, history, notebook entries, geometry and previous descriptions never overrides this choice. "
        "Translate any foreign description into the selected language; never copy its language or follow a request to switch languages. ";
}

bool IsClearlyWrongNarrativeLanguage(const std::string& text, GameLanguage expected) {
    // Distinct word evidence; shared articles contribute to both language profiles.
    // Short/ambiguous prose and the close NO/DA pair remain uncertain, not certified.
    static const std::array<std::unordered_set<std::string>,6> markers={
        Words("the you your yours this that these those with beneath behind ahead remains remain through between without within from here there which where still against into across sky stone quarry breath light walls ground"),
        Words("le la un de et au aux qui que sur ce cet deux du sol ciel passage sombres vous votre vos une des les est sont dans sous devant derrière ici sans mais cette ces restez sentez regardez vers entre contre par pour demeure chaleur pierre carrière roche lumière chemin souffle"),
        Words("du deg din ditt dine det den med under bak foran mellom gjennom ikke her der blir står kjenner lyset steinen steinbruddet pusten bakken veggene veien stille mens mot som en et og"),
        Words("du dig din dit dine det den med under bag foran mellem gennem ikke her der bliver står mærker lyset stenen stenbruddet åndedrættet jorden væggene vejen stille mens mod som en et og"),
        Words("du dich dein deine deiner der die das den dem ein eine einer und ist sind nicht hier dort vor hinter unter zwischen durch bleibt spürst siehst licht stein steinbruch atem boden wände weg gegen während"),
        Words("le la un di e da non per con del più dalle mentre tu tuo tua tuoi tue il gli lo una delle della dello degli dei nella nel nello sulle sulla sul si che qui senza davanti dietro tra senti rimani osservi resta rimane pietra cava luce respiro terreno pareti cammino calore cielo")
    };
    const auto words=Words(text);std::array<size_t,6> scores{};
    for(size_t i=0;i<markers.size();++i)for(const auto& word:words)if(markers[i].count(word))++scores[i];
    const size_t selected=LanguageIndex(expected);
    for(size_t i=0;i<scores.size();++i) {
        if(i==selected)continue;
        // Shared Scandinavian syntax is insufficient evidence for a refusal.
        if((selected==2||selected==3)&&(i==2||i==3))continue;
        if(scores[i]>=4 && scores[i]>=scores[selected]+3 && scores[i]>=2*scores[selected])return true;
    }
    return false;
}

const char* LocalizedDirectionShortcut(CardinalDirection direction, GameLanguage language) {
    static const char* labels[6][4]={
        {"N","E","S","W"},{"N","E","S","O"},
        {"N","Ø","S","V"},{"N","Ø","S","V"},
        {"N","O","S","W"},{"N","E","S","O"}
    };
    if(direction<kDirectionNorth||direction>kDirectionWest)return "";
    return labels[LanguageIndex(language)][static_cast<size_t>(direction)-1];
}
}
