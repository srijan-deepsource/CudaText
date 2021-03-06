(*
This Source Code Form is subject to the terms of the Mozilla Public
License, v. 2.0. If a copy of the MPL was not distributed with this
file, You can obtain one at http://mozilla.org/MPL/2.0/.

Copyright (c) Alexey Torgashin
*)
{$ifdef nn}begin end;{$endif}

procedure TfmMain.FrameOnChangeCaption(Sender: TObject);
var
  Gr: TATGroups;
  Pages: TATPages;
  Frame: TEditorFrame;
  NLocalGroup, NGlobalGroup, NTab: integer;
  D: TATTabData;
begin
  Frame:= Sender as TEditorFrame;
  GetFrameLocation(Frame, Gr, Pages, NLocalGroup, NGlobalGroup, NTab);
  D:= Pages.Tabs.GetTabData(NTab);
  if D=nil then Exit;
  D.TabCaption:= Frame.TabCaption;
  D.TabModified:= Frame.Ed1.Modified or Frame.Ed2.Modified;
  Pages.Invalidate;

  UpdateCaption;
end;

procedure TfmMain.FrameOnUpdateStatus(Sender: TObject);
begin
  FLastStatusbarUpdated:= true;
  //real updating will run on TimerIdle work
end;

procedure TfmMain.DoCodetree_StopUpdate;
var
  F: TEditorFrame;
begin
  F:= CurrentFrame;
  if F=nil then exit;
  F.Adapter[F.Editor].StopTreeUpdate;
end;

procedure TfmMain.FrameOnEditorFocus(Sender: TObject);
var
  F: TEditorFrame;
  Ed: TATSynEdit;
  Tree: TTreeView;
  bTreeCached: boolean;
begin
  Ed:= Sender as TATSynEdit;
  F:= Ed.Parent as TEditorFrame;

  F.GetTabGroups.PagesCurrent:= F.GetTabPages;

  UpdateCaption;
  UpdateTabsActiveColor(F);
  UpdateMenuPlugins_Shortcuts;

  if FNewClickedEditor<>Ed then
  begin
    FNewClickedEditor:= Ed;

    //prevent filling tree from ParseDone in previous focused frame
    TimerTreeFill.Enabled:= false;

    bTreeCached:= false;
    if F.CachedTreeViewInited[Ed] then
    begin
      Tree:= F.CachedTreeView[Ed];
      if (Tree.Items.Count>0) then
      begin
        bTreeCached:= true;
        DoTreeviewCopy(Tree, CodeTree.Tree);
        UpdateTreeFilter;
        UpdateTreeSelection(F, Ed);
      end;
    end;

    if not bTreeCached then
      UpdateTreeContents;
  end;
end;

function TfmMain.GetFrame(AIndex: integer): TEditorFrame;
var
  D: TATTabData;
  NCount: integer;
begin
  Result:= nil;

  //frame can be inside Groups, GroupsF1..GroupsF3
  NCount:= Groups.GetTabTotalCount;
  if AIndex<NCount then
  begin
    D:= Groups.GetTabDataOfTotalIndex(AIndex);
    if Assigned(D) then
      Result:= D.TabObject as TEditorFrame;
    exit;
  end;

  if FloatGroups then
  begin
    Dec(AIndex, NCount);
    NCount:= GroupsF1.GetTabTotalCount;
    if AIndex<NCount then
    begin
      D:= GroupsF1.GetTabDataOfTotalIndex(AIndex);
      if Assigned(D) then
        Result:= D.TabObject as TEditorFrame;
      exit;
    end;

    Dec(AIndex, NCount);
    NCount:= GroupsF2.GetTabTotalCount;
    if AIndex<NCount then
    begin
      D:= GroupsF2.GetTabDataOfTotalIndex(AIndex);
      if Assigned(D) then
        Result:= D.TabObject as TEditorFrame;
      exit;
    end;

    Dec(AIndex, NCount);
    NCount:= GroupsF3.GetTabTotalCount;
    if AIndex<NCount then
    begin
      D:= GroupsF3.GetTabDataOfTotalIndex(AIndex);
      if Assigned(D) then
        Result:= D.TabObject as TEditorFrame;
      exit;
    end;
  end;
end;

function TfmMain.FrameCount: integer;
begin
  Result:= Groups.GetTabTotalCount;
  if FloatGroups then
  begin
    Inc(Result, GroupsF1.GetTabTotalCount);
    Inc(Result, GroupsF2.GetTabTotalCount);
    Inc(Result, GroupsF3.GetTabTotalCount);
  end;
end;

function TfmMain.CurrentGroups: TATGroups;
  //
  function _FormAct(F: TForm): boolean; inline;
  begin
    Result:= Assigned(F)
      and (AppActiveForm=F)
      and F.Visible;
  end;
  //
begin
  Result:= Groups;

  if FloatGroups then
  begin
    if _FormAct(FFormFloatGroups1) then
      Result:= GroupsF1
    else
    if _FormAct(FFormFloatGroups2) then
      Result:= GroupsF2
    else
    if _FormAct(FFormFloatGroups3) then
      Result:= GroupsF3
  end;
end;

function TfmMain.CurrentFrame: TEditorFrame;
var
  Gr: TATGroups;
  Pages: TATPages;
  D: TATTabData;
begin
  Result:= nil;
  Gr:= CurrentGroups;
  Pages:= Gr.PagesCurrent;

  D:= Pages.Tabs.GetTabData(Pages.Tabs.TabIndex);
  if D=nil then Exit;
  if D.TabObject=nil then Exit;
  Result:= D.TabObject as TEditorFrame;
end;

function TfmMain.CurrentEditor: TATSynEdit;
var
  F: TEditorFrame;
begin
  F:= CurrentFrame;
  if F<>nil then
    Result:= F.Editor
  else
    Result:= nil;
end;

procedure TfmMain.SetFrame(Frame: TEditorFrame);
var
  Gr: TATGroups;
  Pages: TATPages;
  NLocalGroup, NGlobalGroup, NTab: integer;
begin
  GetFrameLocation(Frame, Gr, Pages, NLocalGroup, NGlobalGroup, NTab);
  Gr.SetPagesAndTabIndex(NLocalGroup, NTab);
end;

procedure TfmMain.FrameOnSaveFile(Sender: TObject);
var
  F: TEditorFrame;
  fn: string;
begin
  F:= Sender as TEditorFrame;
  fn:= F.FileName;
  MsgStatus(
    msgStatusSavedFile+' '+
    ExtractFileName_Fixed(fn)+
    ' ('+AppCollapseHomeDirInFilename(ExtractFileDir_Fixed(fn))+')'
    );

  //if it's user.json, reread app config (using timer!)
  if SameFileName(fn, AppFile_OptionsUser) then
    CurrentEditor.DoCommand(cmd_OpsReloadAndApply);
end;


procedure TfmMain.FrameOnInitAdapter(Sender: TObject);
begin
  with Sender as TATAdapterEControl do
  begin
    DynamicHiliteEnabled:= EditorOps.OpLexerDynamicHiliteEnabled;
    DynamicHiliteMaxLines:= EditorOps.OpLexerDynamicHiliteMaxLines;

    OnParseDone:= @FrameParseDone;
    OnLexerChange:= @FrameLexerChange;
  end;
end;

procedure TfmMain.FrameParseDone(Sender: TObject);
const
  cMinTime = 600;
var
  Frame: TEditorFrame;
  Params: TAppVariantArray;
begin
  UpdateLexerProgressbar(0, true);
  UpdateTreeContents;

  //restoring of folded state from history is here,
  //after parsing done
  Frame:= (Sender as TATAdapterEControl).Owner as TEditorFrame;
  Frame.DoRestoreFolding(Frame.Editor);

  //do on_lexer_parsed only if parsing was long
  if (Sender as TATAdapterEControl).LexerParsingElapsed>=cMinTime then
  begin
    SetLength(Params, 0);
    DoPyEvent(Frame.Editor, cEventOnLexerParsed, Params);
  end;
end;

function TfmMain.FrameOfPopup: TEditorFrame;
var
  Pages: TATPages;
  NTab: integer;
  D: TATTabData;
begin
  Result:= nil;
  Pages:= GroupsCtx.PopupPages; if Pages=nil then exit;
  NTab:= GroupsCtx.PopupTabIndex; if NTab<0 then exit;
  D:= Pages.Tabs.GetTabData(NTab); if D=nil then exit;
  Result:= D.TabObject as TEditorFrame;
end;

