// (c) by Padreic (Padreic@mg.mud.de)

/* 12 Juli 1998, Padreic
 *
 * Dieser Master dient zum einsparen von call_out Ketten bei Laufnpcs.
 * Anmelden bei diesem Master geschieht ueber die Funktion RegisterWalker().
 * varargs void RegisterWalker(int time, int random, closure walk_closure)
 *   time - in welchen Abstaenden soll das Objekt bewegt werden
 *   rand - dieser Wert wird als random immer beim bewegen zu time addiert
 *   walk_closure - die Closure die immer aufgerufen werden muss, wenn dieser
 *                  Parameter weggelassen wird, wird statdessen die Funktion
 *                  Walk() im NPC aufgerufen.
 *
 * Abgemeldet wird der NPC sobald die Closure bzw. die Walk-Funktion 0
 * returned. Bei allen Werten !=0 bleibt der NPC aktiv.
 *
 * Hinweis: Der NPC muss sich mind. alle 180 sek. bewegen (time+random),
 *          ansonsten kann dieser Master nicht verwendet werden.
 */

#pragma strong_types,rtt_checks
#pragma no_clone,no_inherit

#define MAX_DELAYTIME       90   /* max. delay ist 2*MAX_DELAYTIME */
#define DEFAULT_WALK_DELAY 180   /* ist der billigste Wert :) */
/* Ab welcher Rest-Tickmenge wird die Verarbeitung von Walkers unterbrochen */
#define MAX_JOB_COST    200000

// Funktionen zum vereinfachten Zugriff auf die Komprimierten Daten
// im Walkerarray
#define TIME(t)        (t & 0x00ff)                   /* 8 Bit = 256 */
#define RANDOM(r)     ((r & 0xff00) >> 8)             /* 8 Bit = 256 */
#define WERT(t, r)    ((t & 0x00ff)+((r << 8) & 0xff00))   /* 16 Bit */

// Indizes fuer clients
#define WALK_DELAY   1
#define WALK_CLOSURE 0

nosave int counter;    // Markiert aktuellen Zeitslot im Array walker
nosave < < closure >* >* walker;
// Mapping mit allen registrierten MNPC (als Objekte) als Key und deren
// Zeitdaten (2. wert) und deren walk_closure (1. Wert).
nosave mapping clients = m_allocate(0,2);

public int Registration();

protected void create()
{
  walker=map(allocate(MAX_DELAYTIME+1), #'allocate);
  enable_commands(); // ohne das, kein heart_beat()
}

#define ERROR(x) raise_error(sprintf(x, previous_object()));

// Man muss selbst darauf aufpassen, das sich ein NPC immer nur einmal
// anmeldet, da sonst auch mehrere Paralelle Walk-Ketten laufen!!!
// Am besten nie direkt sondern nur ueber einen Standardnpc benutzen.
// Bemerkung: man kann hiermit andere Objekt registrieren. Aber nur das Objekt
// selber kann spaeter seine Registrierung pruefen oder sich abmelden...
// (Fraglich, ob das so gewollt ist.)
public varargs void RegisterWalker(int time, int rand, closure walk_closure)
{
  // pruefen ob die Paramter zulaessig sind...
  if (time<0) ERROR("negative time to RegisterWalker() from %O.\n");
  if (rand<0) ERROR("negative random to RegisterWalker() from %O.\n");
  if ((time+rand) > (2*MAX_DELAYTIME)) 
    ERROR("Too long delaytime from %s to RegisterWalker().\n");

  if (Registration())
    raise_error(sprintf("Mehrfachanmeldung nicht erlaubt. Objekt: %O\n",
        previous_object()));

  int wert=WERT(time, rand);
  if (!wert && !rand) wert=DEFAULT_WALK_DELAY;

  closure func = walk_closure;
  if (!closurep(func))
  {
    func=symbol_function("Walk", previous_object());
    if (!func)
      raise_error("RegisterWalker() call from Object without Walk() function.\n");
  }
  if (!sizeof(clients)) {
    set_heart_beat(1);
  }
  int next=counter;
  next+=(time+random(rand))/2;
  if (next>MAX_DELAYTIME) next-=MAX_DELAYTIME;
  walker[next]+=({ func });
  clients += ([ get_type_info(func, 2): func; wert ]);
}

// Aufruf nach Moeglichkeit bitte vermeiden, da recht aufwendig. Meist ist
// es leicht im NPC "sauber Buch zu fuehren" und dann ggf. aus Walk() 
// 0 zu returnen.
public void RemoveWalker()
{
  if (!member(clients, previous_object()))
    return;

  for (int i=MAX_DELAYTIME; i>=0; i--) {
    for (int j=sizeof(walker[i])-1; j>=0; j--)
    {
      if (get_type_info(walker[i][j], 2)==previous_object())
      {
        // koennte gerade im heart_beat stecken... Eintrag ersetzen durch was,
        // was beim Abarbeiten ausgetragen wird.
        if (i==counter)
          walker[i][j]=function () {return 0;};
        else
          walker[i][j]=0;
      }
    }
    if (i!=counter) // koennte gerade im heart_beat stecken...
      walker[i]-=({ 0 });
  }
  m_delete(clients, previous_object());
}

public int Registration()
{
  return member(clients, previous_object());
}

void heart_beat()
{
   int i = sizeof(walker[counter]);
   if (i)
   {
     for (i--;i>=0;i--)
     {
       if (get_eval_cost() < MAX_JOB_COST)
       {
         // nicht abgefertigte NPCs im naechsten heart_beat ausfuehren
         walker[counter]=walker[counter][0..i];
         return;
       }
       else
       {
         closure func = walker[counter][i];
         if (func)
         {
           object mnpc = get_type_info(func, 2);
           mixed res;
           if (!catch(res=funcall(func);publish)
               && intp(res) && res)
           {
             // Es gab keinen Fehler und das Objekt will weiterlaufen.
             // Naechsten Zeitslot bestimmen und dort die closure eintragen.
             int delay = clients[mnpc, WALK_DELAY];
             int next = counter + (TIME(delay) + random(RANDOM(delay)))/2;
             if (next > MAX_DELAYTIME)
               next -= MAX_DELAYTIME;
             walker[next] += ({ func });
           }
           else // Fehler oder Objekt will abschalten
             m_delete(clients, mnpc);
         }
         // else: Falls die closure nicht mehr existiert, existiert das Objekt
         // nicht mehr, dann ist es ohnehin auch schon aus clients raus und es
         // muss nix gemacht werden.
       }
     }
     walker[counter]=({}); // fertiger Zeitslot, komplett leeren
   }
   // Wrap-around am Ende des Arrays.
   if (counter == MAX_DELAYTIME)
     counter=0;
   else
     counter++;

   // wenn keine Clients mehr uebrig, kann pausiert werden. Es kann sein, dass
   // noch Dummy-Eintraege in walker enthalten sind, die stoeren nicht, bis
   // wir das naechste Mal drueber laufen.
   if (!sizeof(clients)) {
     set_heart_beat(0);
   }
}

void reset()
// kostet maximal einen unnoetigen heart_beat() pro reset -> vertretbar
// dient zu einem wieder anwerfen im Falle eines Fehlers im heart_beat()
{
  if (set_heart_beat(0)<=0)
  {
    if (sizeof(clients) > 0)
    {
       write_file(object_name()+".err", sprintf(
         "%s: Fehler im heart_beat(). %d aktive Prozesse.\n",
          dtime(time()), sizeof(clients)));
       enable_commands();
       set_heart_beat(1);
    }
  }
  else
    set_heart_beat(1);
}

// Bemerkung: damit kann jeder die Closures ermitteln und dann selber rufen.
mixed *WalkerList() // nur fuer Debugzwecke
{  return ({ clients, walker, counter });  }
