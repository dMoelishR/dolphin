// Copyright 2015 Dolphin Emulator Project
// Licensed under GPLv2+
// Refer to the license.txt file included.

#include <wx/button.h>
#include <wx/checkbox.h>
#include <wx/choice.h>
#include <wx/listbox.h>
#include <wx/notebook.h>
#include <wx/panel.h>
#include <wx/sizer.h>
#include <wx/spinctrl.h>
#include <wx/stattext.h>
#include <wx/textctrl.h>

#include "Common/FileUtil.h"
#include "Common/IniFile.h"
#include "Core/NetPlayClient.h"
#include "Core/NetPlayServer.h"
#include "DolphinWX/Frame.h"
#include "DolphinWX/Main.h"
#include "DolphinWX/NetPlay/NetPlaySetupFrame.h"
#include "DolphinWX/NetPlay/NetWindow.h"
#include "DolphinWX/WxUtils.h"

static void GetTraversalPort(IniFile::Section& section, std::string* port)
{
  section.Get("TraversalPort", port, "6262");
  port->erase(std::remove(port->begin(), port->end(), ' '), port->end());
  if (port->empty())
    *port = "6262";
}

static void GetTraversalServer(IniFile::Section& section, std::string* server)
{
  section.Get("TraversalServer", server, "stun.dolphin-emu.org");
  server->erase(std::remove(server->begin(), server->end(), ' '), server->end());
  if (server->empty())
    *server = "stun.dolphin-emu.org";
}

NetPlaySetupFrame::NetPlaySetupFrame(wxWindow* const parent, const CGameListCtrl* const game_list)
    : wxFrame(parent, wxID_ANY, _("Dolphin NetPlay Setup")), m_game_list(game_list)
{
  IniFile inifile;
  inifile.Load(File::GetUserPath(F_DOLPHINCONFIG_IDX));
  IniFile::Section& netplay_section = *inifile.GetOrCreateSection("NetPlay");

  wxPanel* const panel = new wxPanel(this);
  panel->Bind(wxEVT_CHAR_HOOK, &NetPlaySetupFrame::OnKeyDown, this);

  // top row
  wxBoxSizer* const trav_szr = new wxBoxSizer(wxHORIZONTAL);
  wxBoxSizer* const nick_szr = new wxBoxSizer(wxHORIZONTAL);
  {
    // Connection Config
    wxStaticText* const connectiontype_lbl = new wxStaticText(
        panel, wxID_ANY, _("Connection Type:"), wxDefaultPosition, wxSize(100, -1));

    m_direct_traversal = new wxChoice(panel, wxID_ANY, wxDefaultPosition, wxSize(150, -1));
    m_direct_traversal->Bind(wxEVT_CHOICE, &NetPlaySetupFrame::OnDirectTraversalChoice, this);
    m_direct_traversal->Append(_("Direct Connection"));
    m_direct_traversal->Append(_("Traversal Server"));

    trav_szr->Add(connectiontype_lbl, 0, wxCENTER, 5);
    trav_szr->AddSpacer(5);
    trav_szr->Add(m_direct_traversal, 0, wxCENTER, 5);

    m_trav_reset_btn = new wxButton(panel, wxID_ANY, _("Reset Traversal Settings"),
                                    wxDefaultPosition, wxSize(-1, 25));
    m_trav_reset_btn->Bind(wxEVT_BUTTON, &NetPlaySetupFrame::OnResetTraversal, this);

    trav_szr->AddSpacer(5);

    trav_szr->Add(m_trav_reset_btn, 0, wxRIGHT);

    // Nickname
    wxStaticText* const nick_lbl =
        new wxStaticText(panel, wxID_ANY, _("Nickname:"), wxDefaultPosition, wxSize(100, -1));

    std::string nickname;
    netplay_section.Get("Nickname", &nickname, "Player");

    m_nickname_text =
        new wxTextCtrl(panel, wxID_ANY, StrToWxStr(nickname), wxDefaultPosition, wxSize(150, -1));

    nick_szr->Add(nick_lbl, 0, wxCENTER);
    nick_szr->Add(m_nickname_text, 0, wxALL, 5);

    std::string travChoice;
    netplay_section.Get("TraversalChoice", &travChoice, "direct");
    if (travChoice == "traversal")
    {
      m_direct_traversal->Select(TRAVERSAL_CHOICE);
    }
    else
    {
      m_direct_traversal->Select(DIRECT_CHOICE);
    }

    std::string centralPort;
    GetTraversalPort(netplay_section, &centralPort);
    std::string centralServer;
    GetTraversalServer(netplay_section, &centralServer);

    m_traversal_lbl = new wxStaticText(panel, wxID_ANY, _("Traversal Server:") + " " +
                                                            centralServer + ":" + centralPort);
  }
  // tabs
  m_notebook = new wxNotebook(panel, wxID_ANY);
  wxPanel* const connect_tab = new wxPanel(m_notebook, wxID_ANY);
  m_notebook->AddPage(connect_tab, _("Connect"));
  wxPanel* const host_tab = new wxPanel(m_notebook, wxID_ANY);
  m_notebook->AddPage(host_tab, _("Host"));

  // connect tab
  {
    m_ip_lbl = new wxStaticText(connect_tab, wxID_ANY, _("Host Code :"));

    std::string last_hash_code;
    netplay_section.Get("HostCode", &last_hash_code, "00000000");
    std::string last_ip_address;
    netplay_section.Get("Address", &last_ip_address, "127.0.0.1");
    m_connect_ip_text = new wxTextCtrl(connect_tab, wxID_ANY, StrToWxStr(last_ip_address));
    m_connect_hashcode_text = new wxTextCtrl(connect_tab, wxID_ANY, StrToWxStr(last_hash_code));

    // Will be overridden by OnDirectTraversalChoice, but is necessary
    // so that both inputs do not take up space
    m_connect_hashcode_text->Hide();

    m_client_port_lbl = new wxStaticText(connect_tab, wxID_ANY, _("Port :"));

    // string? w/e
    std::string port;
    netplay_section.Get("ConnectPort", &port, "2626");
    m_connect_port_text = new wxTextCtrl(connect_tab, wxID_ANY, StrToWxStr(port));

    wxButton* const connect_btn = new wxButton(connect_tab, wxID_ANY, _("Connect"));
    connect_btn->Bind(wxEVT_BUTTON, &NetPlaySetupFrame::OnJoin, this);

    wxStaticText* const alert_lbl = new wxStaticText(
        connect_tab, wxID_ANY,
        _("ALERT:\n\n"
          "All players must use the same Dolphin version.\n"
          "All memory cards, SD cards and cheats must be identical between players or disabled.\n"
          "If DSP LLE is used, DSP ROMs must be identical between players.\n"
          "If connecting directly, the host must have the chosen UDP port open/forwarded!\n"
          "\n"
          "Wiimote netplay is experimental and should not be expected to work.\n"));

    wxBoxSizer* const top_szr = new wxBoxSizer(wxHORIZONTAL);

    top_szr->Add(m_ip_lbl, 0, wxALIGN_CENTER_VERTICAL | wxRIGHT, 5);
    top_szr->Add(m_connect_ip_text, 3);
    top_szr->Add(m_connect_hashcode_text, 3);
    top_szr->Add(m_client_port_lbl, 0, wxCENTER | wxRIGHT | wxLEFT, 5);
    top_szr->Add(m_connect_port_text, 1);

    wxBoxSizer* const con_szr = new wxBoxSizer(wxVERTICAL);
    con_szr->Add(top_szr, 0, wxALL | wxEXPAND, 5);
    con_szr->AddStretchSpacer(1);
    con_szr->Add(alert_lbl, 0, wxLEFT | wxRIGHT | wxEXPAND, 5);
    con_szr->AddStretchSpacer(1);
    con_szr->Add(connect_btn, 0, wxALL | wxALIGN_RIGHT, 5);

    connect_tab->SetSizerAndFit(con_szr);
  }

  // host tab
  {
    m_host_port_lbl = new wxStaticText(host_tab, wxID_ANY, _("Port :"));

    // string? w/e
    std::string port;
    netplay_section.Get("HostPort", &port, "2626");
    m_host_port_text = new wxTextCtrl(host_tab, wxID_ANY, StrToWxStr(port));

    m_traversal_listen_port_enabled = new wxCheckBox(host_tab, wxID_ANY, _("Force Listen Port: "));
    m_traversal_listen_port = new wxSpinCtrl(host_tab, wxID_ANY, "", wxDefaultPosition,
                                             wxSize(80, -1), wxSP_ARROW_KEYS, 1, 65535);

    unsigned int listen_port;
    netplay_section.Get("ListenPort", &listen_port, 0);
    m_traversal_listen_port_enabled->SetValue(listen_port != 0);
    m_traversal_listen_port->Enable(m_traversal_listen_port_enabled->IsChecked());
    m_traversal_listen_port->SetValue(listen_port);

    m_traversal_listen_port_enabled->Bind(wxEVT_CHECKBOX,
                                          &NetPlaySetupFrame::OnTraversalListenPortChanged, this);
    m_traversal_listen_port->Bind(wxEVT_TEXT, &NetPlaySetupFrame::OnTraversalListenPortChanged,
                                  this);

    wxButton* const host_btn = new wxButton(host_tab, wxID_ANY, _("Host"));
    host_btn->Bind(wxEVT_BUTTON, &NetPlaySetupFrame::OnHost, this);

    m_game_lbox =
        new wxListBox(host_tab, wxID_ANY, wxDefaultPosition, wxDefaultSize, 0, nullptr, wxLB_SORT);
    m_game_lbox->Bind(wxEVT_LISTBOX_DCLICK, &NetPlaySetupFrame::OnHost, this);

    NetPlayDialog::FillWithGameNames(m_game_lbox, *game_list);

    std::string last_hosted_game;
    if (netplay_section.Get("SelectedHostGame", &last_hosted_game, ""))
      m_game_lbox->SetStringSelection(last_hosted_game);

    wxBoxSizer* const top_szr = new wxBoxSizer(wxHORIZONTAL);
    top_szr->Add(m_host_port_lbl, 0, wxCENTER | wxRIGHT, 5);
    top_szr->Add(m_host_port_text, 0);
#ifdef USE_UPNP
    m_upnp_chk = new wxCheckBox(host_tab, wxID_ANY, _("Forward port (UPnP)"));
    top_szr->Add(m_upnp_chk, 0, wxALL, 5);
#endif
    wxBoxSizer* const bottom_szr = new wxBoxSizer(wxHORIZONTAL);
    bottom_szr->Add(m_traversal_listen_port_enabled, 0, wxCENTER | wxLEFT, 5);
    bottom_szr->Add(m_traversal_listen_port, 0, wxCENTER, 0);
    wxBoxSizer* const host_btn_szr = new wxBoxSizer(wxVERTICAL);
    host_btn_szr->Add(host_btn, 0, wxCENTER | wxALIGN_RIGHT, 0);
    bottom_szr->Add(host_btn_szr, 1, wxALL, 5);

    wxBoxSizer* const host_szr = new wxBoxSizer(wxVERTICAL);
    host_szr->Add(top_szr, 0, wxALL | wxEXPAND, 5);
    host_szr->Add(m_game_lbox, 1, wxLEFT | wxRIGHT | wxEXPAND, 5);
    host_szr->Add(bottom_szr, 0, wxEXPAND, 0);

    host_tab->SetSizerAndFit(host_szr);
  }

  // bottom row
  wxButton* const quit_btn = new wxButton(panel, wxID_ANY, _("Quit"));
  quit_btn->Bind(wxEVT_BUTTON, &NetPlaySetupFrame::OnQuit, this);

  // main sizer
  wxBoxSizer* const main_szr = new wxBoxSizer(wxVERTICAL);
  main_szr->Add(trav_szr, 0, wxALL | wxALIGN_LEFT, 5);
  main_szr->Add(nick_szr, 0, wxALL | wxALIGN_LEFT, 5);
  main_szr->Add(m_traversal_lbl, 0, wxALL | wxALIGN_LEFT, 5);
  main_szr->Add(m_notebook, 1, wxLEFT | wxRIGHT | wxEXPAND, 5);
  main_szr->Add(quit_btn, 0, wxALL | wxALIGN_RIGHT, 5);

  panel->SetSizerAndFit(main_szr);

  // Handle focus on tab changes
  panel->Bind(wxEVT_NOTEBOOK_PAGE_CHANGED, &NetPlaySetupFrame::OnTabChanged, this);

  // wxBoxSizer* const diag_szr = new wxBoxSizer(wxVERTICAL);
  // diag_szr->Add(panel, 1, wxEXPAND);
  // SetSizerAndFit(diag_szr);

  main_szr->SetSizeHints(this);

  Center();
  Show();

  //  Needs to be done last or it set up the spacing on the page correctly
  wxCommandEvent ev;
  OnDirectTraversalChoice(ev);
}

NetPlaySetupFrame::~NetPlaySetupFrame()
{
  IniFile inifile;
  const std::string dolphin_ini = File::GetUserPath(F_DOLPHINCONFIG_IDX);
  inifile.Load(dolphin_ini);
  IniFile::Section& netplay_section = *inifile.GetOrCreateSection("NetPlay");

  std::string travChoice;
  switch (m_direct_traversal->GetSelection())
  {
  case TRAVERSAL_CHOICE:
    travChoice = "traversal";
    break;
  case DIRECT_CHOICE:
    travChoice = "direct";
    break;
  }

  netplay_section.Set("TraversalChoice", travChoice);
  netplay_section.Set("Nickname", WxStrToStr(m_nickname_text->GetValue()));

  if (m_direct_traversal->GetCurrentSelection() == DIRECT_CHOICE)
    netplay_section.Set("Address", WxStrToStr(m_connect_ip_text->GetValue()));
  else
    netplay_section.Set("HostCode", WxStrToStr(m_connect_hashcode_text->GetValue()));

  netplay_section.Set("ConnectPort", WxStrToStr(m_connect_port_text->GetValue()));
  netplay_section.Set("HostPort", WxStrToStr(m_host_port_text->GetValue()));
  netplay_section.Set("ListenPort", m_traversal_listen_port_enabled->IsChecked() ?
                                        m_traversal_listen_port->GetValue() :
                                        0);

  inifile.Save(dolphin_ini);
  main_frame->g_NetPlaySetupDiag = nullptr;
}

void NetPlaySetupFrame::MakeNetPlayDiag(int port, const std::string& game, bool is_hosting)
{
  NetPlayDialog*& npd = NetPlayDialog::GetInstance();
  NetPlayClient*& netplay_client = NetPlayDialog::GetNetPlayClient();

  bool trav = !is_hosting && m_direct_traversal->GetCurrentSelection() == TRAVERSAL_CHOICE;

  std::string ip;
  npd = new NetPlayDialog(m_parent, m_game_list, game, is_hosting);
  if (is_hosting)
    ip = "127.0.0.1";
  else if (trav)
    ip = WxStrToStr(m_connect_hashcode_text->GetValue());
  else
    ip = WxStrToStr(m_connect_ip_text->GetValue());

  IniFile inifile;
  inifile.Load(File::GetUserPath(F_DOLPHINCONFIG_IDX));
  IniFile::Section& netplay_section = *inifile.GetOrCreateSection("NetPlay");

  std::string centralPortString;
  GetTraversalPort(netplay_section, &centralPortString);
  unsigned long int centralPort;
  StrToWxStr(centralPortString).ToULong(&centralPort);

  std::string centralServer;
  GetTraversalServer(netplay_section, &centralServer);

  netplay_client = new NetPlayClient(ip, (u16)port, npd, WxStrToStr(m_nickname_text->GetValue()),
                                     trav, centralServer, (u16)centralPort);
  if (netplay_client->IsConnected())
  {
    int winPosX, winPosY, winWidth, winHeight;

    // Remember the window size and position for NetWindow
    netplay_section.Get("NetWindowPosX", &winPosX, -1);
    netplay_section.Get("NetWindowPosY", &winPosY, -1);
    netplay_section.Get("NetWindowWidth", &winWidth, 768);
    netplay_section.Get("NetWindowHeight", &winHeight, 768 - 128);

    if (winPosX == -1 || winPosY == -1)
    {
      npd->SetSize(768, 768 - 128);
      npd->Center();
    }
    else
    {
      npd->SetSize(winPosX, winPosY, winWidth, winHeight);
    }

    npd->Show();
    Destroy();
  }
  else
  {
    npd->Destroy();
  }
}

void NetPlaySetupFrame::OnHost(wxCommandEvent&)
{
  DoHost();
}

void NetPlaySetupFrame::DoHost()
{
  IniFile ini_file;
  const std::string dolphin_ini = File::GetUserPath(F_DOLPHINCONFIG_IDX);
  ini_file.Load(dolphin_ini);
  IniFile::Section& netplay_section = *ini_file.GetOrCreateSection("NetPlay");

  NetPlayDialog*& npd = NetPlayDialog::GetInstance();
  NetPlayServer*& netplay_server = NetPlayDialog::GetNetPlayServer();

  if (npd)
  {
    WxUtils::ShowErrorDialog(_("A NetPlay window is already open!"));
    return;
  }

  if (m_game_lbox->GetSelection() == wxNOT_FOUND)
  {
    WxUtils::ShowErrorDialog(_("You must choose a game!"));
    return;
  }

  std::string game(WxStrToStr(m_game_lbox->GetStringSelection()));

  bool trav;
  unsigned long listen_port = 0;
  if (m_direct_traversal->GetCurrentSelection() == TRAVERSAL_CHOICE)
  {
    trav = true;
    listen_port =
        m_traversal_listen_port_enabled->IsChecked() ? m_traversal_listen_port->GetValue() : 0;
  }
  else
  {
    trav = false;
    m_host_port_text->GetValue().ToULong(&listen_port);
  }

  std::string centralPortString;
  GetTraversalPort(netplay_section, &centralPortString);
  unsigned long int centralPort;
  StrToWxStr(centralPortString).ToULong(&centralPort);

  std::string centralServer;
  GetTraversalServer(netplay_section, &centralServer);

  netplay_server = new NetPlayServer((u16)listen_port, trav, centralServer, (u16)centralPort);
  if (netplay_server->is_connected)
  {
    netplay_server->ChangeGame(game);
    netplay_server->AdjustPadBufferSize(INITIAL_PAD_BUFFER_SIZE);
#ifdef USE_UPNP
    if (m_upnp_chk->GetValue())
      netplay_server->TryPortmapping(listen_port);
#endif
    MakeNetPlayDiag(netplay_server->GetPort(), game, true);
    netplay_server->SetNetPlayUI(NetPlayDialog::GetInstance());

    netplay_section.Set("SelectedHostGame", game);
    ini_file.Save(dolphin_ini);
  }
  else
  {
    if (trav && m_traversal_listen_port_enabled->IsChecked())
      WxUtils::ShowErrorDialog(
          _("Failed to listen. Someone is probably already listening on the port you specified."));
    else
      WxUtils::ShowErrorDialog(
          _("Failed to listen. Is another instance of the NetPlay server running?"));
  }
}

void NetPlaySetupFrame::OnJoin(wxCommandEvent&)
{
  DoJoin();
}

void NetPlaySetupFrame::DoJoin()
{
  NetPlayDialog*& npd = NetPlayDialog::GetInstance();
  if (npd)
  {
    WxUtils::ShowErrorDialog(_("A NetPlay window is already open!"));
    return;
  }

  unsigned long port = 0;
  m_connect_port_text->GetValue().ToULong(&port);
  MakeNetPlayDiag(port, "", false);
}

void NetPlaySetupFrame::OnResetTraversal(wxCommandEvent& event)
{
  IniFile inifile;
  const std::string dolphin_ini = File::GetUserPath(F_DOLPHINCONFIG_IDX);
  inifile.Load(dolphin_ini);
  IniFile::Section& netplay_section = *inifile.GetOrCreateSection("NetPlay");
  netplay_section.Set("TraversalServer", (std::string) "stun.dolphin-emu.org");
  netplay_section.Set("TraversalPort", (std::string) "6262");
  inifile.Save(dolphin_ini);

  m_traversal_lbl->SetLabelText(_("Traversal: ") + "stun.dolphin-emu.org:6262");
}

void NetPlaySetupFrame::OnTraversalListenPortChanged(wxCommandEvent& event)
{
  m_traversal_listen_port->Enable(m_traversal_listen_port_enabled->IsChecked());
}

void NetPlaySetupFrame::OnDirectTraversalChoice(wxCommandEvent& event)
{
  int sel = m_direct_traversal->GetSelection();
  IniFile inifile;
  inifile.Load(File::GetUserPath(F_DOLPHINCONFIG_IDX));
  IniFile::Section& netplay_section = *inifile.GetOrCreateSection("NetPlay");

  if (sel == TRAVERSAL_CHOICE)
  {
    m_traversal_lbl->Show();
    m_trav_reset_btn->Show();
    m_connect_hashcode_text->Show();
    m_connect_ip_text->Hide();
    // Traversal
    // client tab
    {
      m_ip_lbl->SetLabelText("Host Code: ");
      m_client_port_lbl->Hide();
      m_connect_port_text->Hide();
    }

    // server tab
    {
      m_host_port_lbl->Hide();
      m_host_port_text->Hide();
      m_traversal_listen_port->Show();
      m_traversal_listen_port_enabled->Show();
#ifdef USE_UPNP
      m_upnp_chk->Hide();
#endif
    }
  }
  else
  {
    m_traversal_lbl->Hide();
    m_trav_reset_btn->Hide();
    m_connect_hashcode_text->Hide();
    m_connect_ip_text->Show();
    // Direct
    // Client tab
    {
      m_ip_lbl->SetLabelText("IP Address :");

      std::string address;
      netplay_section.Get("Address", &address, "127.0.0.1");
      m_connect_ip_text->SetLabelText(address);

      m_client_port_lbl->Show();
      m_connect_port_text->Show();
    }

    // Server tab
    m_traversal_listen_port->Hide();
    m_traversal_listen_port_enabled->Hide();
    m_host_port_lbl->Show();
    m_host_port_text->Show();
#ifdef USE_UPNP
    m_upnp_chk->Show();
#endif
  }
  m_connect_ip_text->GetParent()->Layout();
  DispatchFocus();
}

void NetPlaySetupFrame::OnKeyDown(wxKeyEvent& event)
{
  // Let the event propagate
  event.Skip();

  if (event.GetKeyCode() != wxKeyCode::WXK_RETURN)
    return;

  int current_tab = m_notebook->GetSelection();

  switch (current_tab)
  {
  case CONNECT_TAB:
    DoJoin();
    break;
  case HOST_TAB:
    DoHost();
    break;
  }
}

void NetPlaySetupFrame::OnTabChanged(wxCommandEvent& event)
{
  // Propagate event
  event.Skip();

  // Delaying action so the first tab order element doesn't override the focus
  m_notebook->Bind(wxEVT_IDLE, &NetPlaySetupFrame::OnAfterTabChange, this);
}

void NetPlaySetupFrame::OnAfterTabChange(wxIdleEvent&)
{
  // Unbinding so we don't hog the idle event
  m_notebook->Unbind(wxEVT_IDLE, &NetPlaySetupFrame::OnAfterTabChange, this);

  DispatchFocus();
}

void NetPlaySetupFrame::DispatchFocus()
{
  int current_tab = m_notebook->GetSelection();

  switch (current_tab)
  {
  case CONNECT_TAB:
    if (m_direct_traversal->GetCurrentSelection() == TRAVERSAL_CHOICE)
      m_connect_hashcode_text->SetFocus();
    else
      m_connect_ip_text->SetFocus();
    break;

  case HOST_TAB:
    m_game_lbox->SetFocus();
    break;
  }
}

void NetPlaySetupFrame::OnQuit(wxCommandEvent&)
{
  Destroy();
}
