#pragma no_shadow, no_inherit
#pragma strong_types,save_types,rtt_checks
#pragma pedantic,range_check,warn_deprecated
#pragma warn_empty_casts,warn_missing_return,warn_function_inconsistent

inherit "/std/secure_thing";

#include <properties.h>
#include <defines.h>

#define BARCHIV "/d/erzmagier/boing/balance/barchives"

#define BS(x) break_string(x,78,0,BS_LEAVE_MY_LFS)
#define BSI(x) break_string(x, 78, y, BS_LEAVE_MY_LFS|BS_INDENT_ONCE)

private varargs void print_map(mapping tmp,int cutoff);

protected void create()
{
  ::create();
  SetProp(P_NAME,"Balance-Tool");
  SetProp(P_AUTOLOADOBJ,1);
  SetProp(P_NODROP,1);
  SetProp(P_NEVERDROP,1);
  AddId(({"btool","balancetool","balance-tool"}));
  SetProp(P_SHORT,"Ein Balance-Tool light");
  SetProp(P_LONG,
    BS("Dies ist das Balance-Tool light. Es versteht folgende Befehle:")+
    BSI("btop <n>:\nZeigt den letzten genehmigten Antrag zu Top n.","- ")+
    BSI("bsuch [-s] <str>:\nSucht case-sensitiv nach str, -s bricht jeden "
    "Eintrag nach 78 Zeichen ab. str darf eine RegExp sein.", "- ")+
    BSI("binhalt [-s] [uid]:\nZeigt den gesamten Inhalt des Balancearchives, "
    "-s bricht jeden Eintrag nach 78 Zeichen ab, uid filtert auf "
    "_vollstaendige_ uids.", "- ")+"\n"+
    BS("Es kann vorkommen, dass Eintraege der falschen UID zugeordnet sind, "
    "oder dass die Genehmigung nicht sehr aussagekraeftig ist, in diesem "
    "Fall bitte eine Mail an das Balanceteam schreiben."));
  AddCmd("btop",
    function int(string str)
    {
      int n;
      if(!str || sscanf(str,"%d",n)!=1)
      {
        notify_fail(
          BS("Syntax: btop <n>"));
        return 0;
      }
      this_interactive()->ReceiveMsg(
        BARCHIV->GetLightAnnounce(n),
        MT_NOTIFICATION|MSG_BS_LEAVE_LFS);
      return 1;
    });
  AddCmd("bsuch",
    function int()
    {
      string str=this_interactive()->_unparsed_args();
      if(!str || !sizeof(str))
      {
        notify_fail(
          BS("Syntax: bsuch <str>"));
        return 0;
      }
      int cutoff;
      string* arr=old_explode(str," ");
      
      if(arr[0]=="-s")
      {
        cutoff=1;
        str=implode(arr[1..]," ");
      }
      mapping tmp=BARCHIV->SearchIndex(str);
      if(!sizeof(tmp))
      {
        this_interactive()->ReceiveMsg(
          "Keine Eintraege gefunden.",
          MT_NOTIFICATION);
      }
      else
      {
        print_map(tmp,cutoff);
      }
      return 1;
    });
  AddCmd("binhalt",
    function int(string str)
    {
      int cutoff;
      string check;
      
      mapping tmp=BARCHIV->GetIndexForWizards();
      if(sizeof(str))
      {
        string* arr=old_explode(str," ");
      
        if(arr[0]=="-s")
        {
          cutoff=1;
          if(sizeof(arr)>=2)
          {
            check=arr[1];
          }
        }
        else
        {
          check=arr[0];
        }
        
        if(sizeof(check))
        {
          tmp=filter(tmp,
            function int(int key, <int|string>* values)
            {
              return values[2]==check;
            });
        }
      }

      print_map(tmp,cutoff);
      return 1;
    });
}

private varargs void print_map(mapping tmp,int cutoff)
{
	if (!mappingp(tmp) || !sizeof(tmp))
	{
		this_interactive()->ReceiveMsg("Keine Daten vorhanden.\n");
		return;
	}
  int* sort=sort_array(m_indices(tmp),#'>);
  string ret="";
  foreach(int i : sort)
  {
    string str=sprintf(" %4d:  %s {%s} (%s)",i,tmp[i,0],tmp[i,2],
      strftime("%d.%m.%y",tmp[i,1]));
    if(cutoff)
    {
      ret+=BS(str[0..77]);
    }
    else
    {
      ret+=BS(str);
    }
  }
  this_interactive()->More(ret);
}
