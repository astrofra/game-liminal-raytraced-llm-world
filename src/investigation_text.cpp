#include "investigation.h"
#include <algorithm>

namespace liminal {
namespace {
std::string Text(GameLanguage l,const char* en,const char* fr,const char* no,const char* da,const char* de,const char* it) {
    const char* values[]={en,fr,no,da,de,it};const int i=static_cast<int>(l);return values[i>=0&&i<6?i:0];
}
std::string PlaceReference(const SessionState& s,const std::string& place) {
    auto label=DescribePlaceLabel(s,place);
    for(size_t i=0;i<s.generated_rooms.size();++i)if(s.generated_rooms[i].room_id==place)
        return label+" — "+Text(s.language,"place ","lieu ","sted ","sted ","Ort ","luogo ")+std::to_string(i+1);
    return label;
}
}
std::string InvestigationMandate(GameLanguage l) {
    return Text(l,
        "Vey, a prospector, is missing. Your expedition must trace his passage, check reports of invisible obstructions and collect a crystal specimen. These are questions to investigate, not established discoveries. Your notebook keeps evidence separate from hypotheses.",
        "Le prospecteur Vey est porté disparu. Votre expédition doit retrouver sa piste, vérifier les signalements d'obstacles invisibles et prélever un spécimen cristallin. Ce sont des questions à explorer, pas encore des découvertes. Votre carnet distingue les constats des hypothèses.",
        "Prospektøren Vey er savnet. Ekspedisjonen skal følge sporene hans, undersøke meldinger om usynlige hindringer og samle en krystallprøve. Dette er spørsmål, ikke funn. Notatboken skiller bevis fra hypoteser.",
        "Prospektøren Vey er savnet. Ekspeditionen skal følge hans spor, undersøge meldinger om usynlige hindringer og indsamle en krystalprøve. Dette er spørgsmål, ikke fund. Notesbogen skelner mellem beviser og hypoteser.",
        "Der Prospektor Vey wird vermisst. Deine Expedition soll seine Spur verfolgen, Berichte über unsichtbare Hindernisse prüfen und eine Kristallprobe sammeln. Dies sind Fragen, noch keine Entdeckungen. Dein Notizbuch trennt Befunde und Hypothesen.",
        "Il prospettore Vey è scomparso. La spedizione deve seguirne le tracce, verificare le segnalazioni di ostacoli invisibili e raccogliere un campione cristallino. Sono domande, non scoperte già accertate. Il taccuino distingue i fatti dalle ipotesi.");
}
std::string InvestigationHint(const std::string& id,GameLanguage l) {
    if(id=="crystal_sample")return Text(l,"crystalline specimen","spécimen cristallin","krystallprøve","krystalprøve","Kristallprobe","campione cristallino");
    if(id=="vey_equipment")return Text(l,"small sampling tool with an identification plate","petit outil de prélèvement avec une plaquette d'identification","lite prøveverktøy med et identifikasjonsmerke","lille prøveværktøj med et identifikationsmærke","kleines Probenwerkzeug mit Kennschild","piccolo utensile da campionamento con targhetta identificativa");
    if(id=="vey_survey")return Text(l,"engraved survey tablet","plaquette de relevé gravée","gravert måleplate","graveret opmålingsplade","gravierte Vermessungstafel","tavoletta di rilevamento incisa");
    return Text(l,"the contacted invisible obstruction","l'obstruction invisible rencontrée","den usynlige hindringen du traff","den usynlige hindring du ramte","das berührte unsichtbare Hindernis","l'ostacolo invisibile incontrato");
}
std::string InvestigationFact(const std::string& id,GameLanguage l) {
    if(id=="crystal_source")return Text(l,
        "The examined specimen matches the expedition's crystal reference. Its role here is unknown.",
        "Le spécimen examiné correspond à la référence cristalline de l'expédition. Son rôle ici reste inconnu.",
        "Den undersøkte prøven samsvarer med ekspedisjonens krystallreferanse. Rollen her er ukjent.",
        "Den undersøgte prøve svarer til ekspeditionens krystalreference. Dens rolle her er ukendt.",
        "Die untersuchte Probe entspricht der Kristallreferenz der Expedition. Ihre Rolle hier ist unbekannt.",
        "Il campione esaminato corrisponde al riferimento cristallino della spedizione. Il suo ruolo qui è ignoto.");
    if(id=="crystal_acquired")return Text(l,
        "You collected the identified crystal specimen. This acquisition remains recorded even if you leave it elsewhere.",
        "Vous avez prélevé le spécimen cristallin identifié. Cette acquisition reste consignée même si vous le déposez ailleurs.",
        "Du har samlet den identifiserte krystallprøven. Funnet forblir registrert selv om du legger prøven et annet sted.",
        "Du har indsamlet den identificerede krystalprøve. Fundet forbliver registreret, selv hvis du lægger prøven et andet sted.",
        "Du hast die identifizierte Kristallprobe aufgenommen. Dieser Fund bleibt vermerkt, auch wenn du sie anderswo ablegst.",
        "Hai raccolto il campione cristallino identificato. Il ritrovamento resta registrato anche se lo depositi altrove.");
    if(id=="vey_equipment")return Text(l,
        "The tool's plate matches Vey's expedition equipment record; crystal residue remains in its jaws. This establishes a trace of his prospecting, not his fate.",
        "La plaquette de l'outil correspond au registre du matériel confié à Vey ; ses mâchoires portent un résidu cristallin. C'est une trace de sa prospection, pas une preuve de son sort.",
        "Verktøyets merke stemmer med utstyret tildelt Vey; kjevene har krystallrester. Dette er et spor etter letingen hans, ikke bevis på skjebnen hans.",
        "Værktøjets mærke stemmer med udstyret tildelt Vey; kæberne har krystalrester. Dette er et spor efter hans efterforskning, ikke bevis for hans skæbne.",
        "Das Kennschild entspricht Veys zugeteilter Ausrüstung; in den Backen haften Kristallreste. Das belegt eine Spur seiner Prospektion, nicht sein Schicksal.",
        "La targhetta corrisponde all'attrezzatura assegnata a Vey; le ganasce conservano residui cristallini. È una traccia della sua prospezione, non una prova del suo destino.");
    if(id=="vey_survey")return Text(l,
        "The tablet bears Vey's authenticated survey mark: he recorded a crystal sampling point, then a departure from it. His later destination is not recorded.",
        "La plaquette porte le repère de relevé authentifié de Vey : il a consigné un point de prélèvement cristallin, puis son départ de ce point. Sa destination ultérieure n'y figure pas.",
        "Platen bærer Veys bekreftede målemerke: han registrerte et sted for krystallprøver og deretter avreisen derfra. Det senere målet er ikke registrert.",
        "Pladen bærer Veys bekræftede opmålingsmærke: han registrerede et sted for krystalprøver og derefter afrejsen derfra. Det senere mål er ikke registreret.",
        "Die Tafel trägt Veys bestätigtes Vermessungszeichen: Er verzeichnete einen Kristallprobenort und danach seinen Aufbruch von dort. Sein späteres Ziel ist nicht vermerkt.",
        "La tavoletta reca il contrassegno autenticato di Vey: registrò un punto di prelievo cristallino, poi la partenza da quel punto. La destinazione successiva non è indicata.");
    if(id=="obstruction_contact")return Text(l,
        "A movement attempt met an impassable, invisible plane along a visibly clear approach. You remained in the same place; its cause is unknown.",
        "Une tentative de déplacement a rencontré un plan invisible infranchissable dans une approche visuellement dégagée. Vous êtes resté au même endroit ; sa cause est inconnue.",
        "Et bevegelsesforsøk traff et usynlig, ugjennomtrengelig plan langs en synlig åpen vei. Du ble på samme sted; årsaken er ukjent.",
        "Et bevægelsesforsøg ramte et usynligt, uigennemtrængeligt plan langs en synligt åben vej. Du blev samme sted; årsagen er ukendt.",
        "Ein Bewegungsversuch traf in einem sichtbar freien Zugang auf eine unsichtbare, unpassierbare Ebene. Du bliebst am selben Ort; die Ursache ist unbekannt.",
        "Un tentativo di movimento ha incontrato un piano invisibile invalicabile lungo un accesso visibilmente libero. Sei rimasto nello stesso luogo; la causa è ignota.");
    if(id=="obstruction_examined")return Text(l,
        "Examining the contacted approach confirms no opaque mass explains the contact. An obstruction is verified; its material, extent and purpose remain unknown.",
        "L'examen de l'approche confirme qu'aucune masse opaque n'explique le contact. L'obstruction est vérifiée ; sa matière, son étendue et sa fonction restent inconnues.",
        "Undersøkelsen av veien bekrefter at ingen ugjennomsiktig masse forklarer kontakten. Hindringen er bekreftet; materiale, utstrekning og formål er ukjent.",
        "Undersøgelsen af vejen bekræfter, at ingen uigennemsigtig masse forklarer kontakten. Hindringen er bekræftet; materiale, udstrækning og formål er ukendt.",
        "Die Untersuchung des Zugangs bestätigt, dass keine undurchsichtige Masse den Kontakt erklärt. Das Hindernis ist bestätigt; Material, Ausdehnung und Zweck bleiben unbekannt.",
        "L'esame dell'accesso conferma che nessuna massa opaca spiega il contatto. L'ostacolo è verificato; materiale, estensione e scopo restano ignoti.");
    return {};
}
std::string InvestigationHypothesisText(const std::string& id,GameLanguage l) {
    if(id=="vey_prospecting")return Text(l,"Vey was prospecting for these crystals.","Vey prospectait ces cristaux.","Vey lette etter disse krystallene.","Vey ledte efter disse krystaller.","Vey suchte nach diesen Kristallen.","Vey cercava questi cristalli.");
    return Text(l,"Vey remained at his sampling point.","Vey serait resté à son point de prélèvement.","Vey ble ved prøvestedet sitt.","Vey blev ved sit prøvested.","Vey blieb an seinem Probenort.","Vey sarebbe rimasto al punto di prelievo.");
}
std::string InvestigationJournal(const SessionState& s) {
    const auto l=s.language;
    std::string out=Text(l,"Established","Établi","Fastslått","Fastslået","Festgestellt","Accertato")+":\n";
    if(s.investigation.evidence.empty())out+=Text(l,"No evidence yet.","Aucune preuve pour le moment.","Ingen bevis ennå.","Ingen beviser endnu.","Noch keine Belege.","Nessuna prova per ora.")+"\n";
    for(const auto& e:s.investigation.evidence) {
        out+=InvestigationFact(e.id,l)+" ("+PlaceReference(s,e.place_id)+")\n";
        if(e.id=="crystal_acquired") {
            out+=Text(l,"Specimen now: ","Spécimen actuellement : ","Prøven nå: ","Prøven nu: ","Probe jetzt: ","Campione attualmente: ");
            if(std::find(s.hard_state.inventory_items.begin(),s.hard_state.inventory_items.end(),e.object_id)!=s.hard_state.inventory_items.end())
                out+=Text(l,"carried","porté","medbrakt","medbragt","mitgeführt","trasportato");
            else for(const auto& room:s.objects.deposits)for(const auto& p:room.second)if(p.object_id==e.object_id)out+=PlaceReference(s,room.first);
            out+=".\n";
        }
    }
    out+="\n"+Text(l,"Hypotheses","Supposé","Hypoteser","Hypoteser","Hypothesen","Ipotesi")+":\n";
    bool any=false;
    for(const auto& h:s.investigation.hypotheses) {
        if(h.supporting_evidence.empty()&&h.contradicting_evidence.empty())continue;
        any=true;out+=InvestigationHypothesisText(h.id,l)+" — ";
        out+=h.status=="supported"?Text(l,"supported, not proven","soutenue, non démontrée","støttet, ikke bevist","støttet, ikke bevist","gestützt, nicht bewiesen","sostenuta, non dimostrata"):
            h.status=="contradicted"?Text(l,"contradicted","contredite","motsagt","modsagt","widerlegt","contraddetta"):
            Text(l,"open","ouverte","åpen","åben","offen","aperta");
        out+=".\n";
    }
    if(!any)out+=Text(l,"No comparison established.","Aucun rapprochement établi.","Ingen sammenheng fastslått.","Ingen sammenhæng fastslået.","Noch kein Zusammenhang festgestellt.","Nessun collegamento accertato.")+"\n";
    out+="\n"+Text(l,"To investigate: Vey's fate and destination; the crystals' role; the obstruction's cause.",
        "À vérifier : le sort et la destination de Vey ; le rôle des cristaux ; la cause de l'obstruction.",
        "Å undersøke: Veys skjebne og mål; krystallenes rolle; årsaken til hindringen.",
        "At undersøge: Veys skæbne og mål; krystallernes rolle; årsagen til hindringen.",
        "Zu prüfen: Veys Schicksal und Ziel; die Rolle der Kristalle; die Ursache des Hindernisses.",
        "Da verificare: destino e destinazione di Vey; ruolo dei cristalli; causa dell'ostacolo.");
    return out;
}
}
