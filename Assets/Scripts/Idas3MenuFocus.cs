using System;
using System.Collections.Generic;
using UnityEngine;

// Rebuild from visible enabled controls; queued confirmation is tied to the
// control's identity, so an asynchronous lobby change cannot click its neighbour.
internal sealed class Idas3MenuFocus
{
    internal struct Item { public string id; public Rect rect; }
    private readonly List<Item> visible=new List<Item>(),building=new List<Item>();
    private string selected,activate;
    private bool armed,wasConfirm,wasBack;
    private int axis;
    private double nextRepeat;
    internal string Selected=>selected;
    internal int Count=>visible.Count;
    internal bool SpatialVertical;
    internal void Reset(){selected=activate=null;armed=false;axis=0;visible.Clear();building.Clear();}
    internal void Begin(){building.Clear();}
    internal bool Control(string id,Rect rect,bool enabled,bool repaint)
    {
        if(enabled)building.Add(new Item{id=id,rect=rect});
        if(enabled&&activate==id&&repaint){activate=null;return true;}
        return false;
    }
    internal void End(){
        visible.Clear();visible.AddRange(building);
        if(!visible.Exists(x=>x.id==selected))selected=visible.Count>0?visible[0].id:null;
        if(!visible.Exists(x=>x.id==activate))activate=null;
    }
    internal bool Focused(string id)=>selected==id;
    internal void Pointer(string id=null){activate=null;armed=false;axis=0;if(id!=null)selected=id;}
    internal bool Poll(int horizontal,int vertical,bool confirm,bool back,bool blocked,double now)
    {
        if(blocked){armed=false;activate=null;axis=0;wasConfirm=confirm;wasBack=back;return false;}
        if(!armed){if(!confirm&&!back&&horizontal==0&&vertical==0)armed=true;wasConfirm=confirm;wasBack=back;return false;}
        bool cancel=back&&!wasBack;
        if(confirm&&!wasConfirm)activate=selected;
        wasConfirm=confirm;wasBack=back;
        int direction=vertical!=0?Math.Sign(vertical):Math.Sign(horizontal)*2;
        if(direction!=0&&(direction!=axis||now>=nextRepeat)){
            Move(direction);nextRepeat=now+(direction!=axis?.32:.10);
        }
        axis=direction;return cancel;
    }
    private void Move(int direction){
        if(visible.Count==0)return;
        int at=visible.FindIndex(x=>x.id==selected);if(at<0)at=0;
        // Vertical traverses every action, including scroll-list entries.
        // Horizontal favours the nearest control in that screen direction.
        int next=(at+(direction>0?1:-1)+visible.Count)%visible.Count;
        if(Math.Abs(direction)==2||SpatialVertical){
            var origin=visible[at].rect.center;float best=float.MaxValue;
            for(int i=0;i<visible.Count;++i){var d=visible[i].rect.center-origin;
                bool horizontal=Math.Abs(direction)==2;
                if((horizontal?d.x:d.y)*Math.Sign(direction)<=1)continue;
                float cost=horizontal?Math.Abs(d.x)+Math.Abs(d.y)*3:Math.Abs(d.y)+Math.Abs(d.x)*3;
                if(cost<best){best=cost;next=i;}
            }
        }
        selected=visible[next].id;activate=null;
    }
}
