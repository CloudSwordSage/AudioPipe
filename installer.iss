[Tasks]
Name: install_vbcable; \
  Description: "{cm:VBCableTaskDesc}"; \
  Check: VBCableNeedsInstall; \
  Flags: unchecked

[CustomMessages]
VBCableTaskDesc=安装 VB-CABLE 虚拟音频设备（必须安装）

[Code]

const
  VB_CABLE_URL    = 'https://download.vb-audio.com/Download_CABLE/VBCABLE_Driver_Pack45.zip';
  VB_CABLE_SHA256 = 'b950e39f01af1d04ea623c8f6d8eb9b6ea5c477c637295fabf20631c85116bfb';

{ ------------------------------------------------------------------ }
{ 检测 VB-CABLE 是否已安装                                            }
{ 查 HKLM\SYSTEM\CurrentControlSet\Services\VBAudioVACMME 服务键      }
{ ------------------------------------------------------------------ }
function VBCableInstalled: Boolean;
begin
  Result := RegKeyExists(HKLM,
    'SYSTEM\CurrentControlSet\Services\VBAudioVACMME');
end;

{ [Tasks] Check: 回调 —— 返回 True 则显示并默认勾选，False 则隐藏 }
function VBCableNeedsInstall: Boolean;
begin
  Result := not VBCableInstalled;
end;

{ ------------------------------------------------------------------ }
{ SHA-256 验证                                                         }
{ ------------------------------------------------------------------ }
function VerifySHA256(const FilePath, ExpectedHash: string): Boolean;
var
  ResultCode: Integer;
  Output: string;
begin
  Exec(ExpandConstant('{sys}\cmd.exe'),
    '/C powershell -NoProfile -NonInteractive -Command ' +
    '"(Get-FileHash -Algorithm SHA256 -LiteralPath ''' + FilePath + ''').Hash.ToLower()"' +
    ' > "' + FilePath + '.sha256.tmp"',
    '', SW_HIDE, ewWaitUntilTerminated, ResultCode);

  if not LoadStringFromFile(FilePath + '.sha256.tmp', Output) then
  begin
    Result := False;
    Exit;
  end;
  DeleteFile(FilePath + '.sha256.tmp');
  Result := (Lowercase(Trim(Output)) = Lowercase(ExpectedHash));
end;

{ ------------------------------------------------------------------ }
{ 下载文件                                                             }
{ ------------------------------------------------------------------ }
function DownloadFile(const URL, DestPath: string): Boolean;
var
  ResultCode: Integer;
begin
  Result := Exec(ExpandConstant('{sys}\cmd.exe'),
    '/C powershell -NoProfile -NonInteractive -Command ' +
    '"Invoke-WebRequest -Uri ''' + URL + ''' -OutFile ''' + DestPath + ''' -UseBasicParsing"',
    '', SW_HIDE, ewWaitUntilTerminated, ResultCode)
    and (ResultCode = 0);
end;

{ ------------------------------------------------------------------ }
{ 解压 ZIP                                                             }
{ ------------------------------------------------------------------ }
function ExtractZip(const ZipPath, DestDir: string): Boolean;
var
  ResultCode: Integer;
begin
  Result := Exec(ExpandConstant('{sys}\cmd.exe'),
    '/C powershell -NoProfile -NonInteractive -Command ' +
    '"Expand-Archive -LiteralPath ''' + ZipPath + ''' -DestinationPath ''' + DestDir + ''' -Force"',
    '', SW_HIDE, ewWaitUntilTerminated, ResultCode)
    and (ResultCode = 0);
end;

{ ------------------------------------------------------------------ }
{ 主安装流程                                                           }
{ ------------------------------------------------------------------ }
procedure InstallVBCable;
var
  ZipPath, ExtractDir, SetupExe: string;
  ResultCode: Integer;
begin
  ZipPath    := ExpandConstant('{app}\VB_CABLE_Driver.zip');
  ExtractDir := ExpandConstant('{app}\VB_CABLE');

  { 下载 }
  if not DownloadFile(VB_CABLE_URL, ZipPath) then
  begin
    MsgBox(
      'VB-CABLE 驱动下载失败，请手动前往以下地址下载并安装：' + #13#10 + VB_CABLE_URL,
      mbError, MB_OK);
    ShellExec('open', VB_CABLE_URL, '', '', SW_SHOWNORMAL, ewNoWait, ResultCode);
    Exit;
  end;

  { SHA-256 校验 }
  if not VerifySHA256(ZipPath, VB_CABLE_SHA256) then
  begin
    DeleteFile(ZipPath);
    MsgBox(
      'VB-CABLE 驱动文件 SHA-256 校验失败，文件可能已损坏。' + #13#10 +
      '请手动前往以下地址下载并安装：' + #13#10 + VB_CABLE_URL,
      mbError, MB_OK);
    ShellExec('open', VB_CABLE_URL, '', '', SW_SHOWNORMAL, ewNoWait, ResultCode);
    Exit;
  end;

  { 解压 }
  if not ExtractZip(ZipPath, ExtractDir) then
  begin
    DeleteFile(ZipPath);
    MsgBox(
      'VB-CABLE 驱动解压失败。' + #13#10 +
      '请手动前往以下地址下载并安装：' + #13#10 + VB_CABLE_URL,
      mbError, MB_OK);
    ShellExec('open', VB_CABLE_URL, '', '', SW_SHOWNORMAL, ewNoWait, ResultCode);
    Exit;
  end;

  DeleteFile(ZipPath);

  { 选择架构对应安装程序 }
  if IsWin64 then
    SetupExe := ExtractDir + '\VBCABLE_Setup_x64.exe'
  else
    SetupExe := ExtractDir + '\VBCABLE_Setup.exe';

  if not FileExists(SetupExe) then
  begin
    MsgBox(
      'VB-CABLE 安装程序未找到：' + SetupExe + #13#10 +
      '请手动前往以下地址下载并安装：' + #13#10 + VB_CABLE_URL,
      mbError, MB_OK);
    ShellExec('open', VB_CABLE_URL, '', '', SW_SHOWNORMAL, ewNoWait, ResultCode);
    Exit;
  end;

  Exec(SetupExe, '/S', ExtractDir, SW_HIDE, ewWaitUntilTerminated, ResultCode);
  if ResultCode <> 0 then
    MsgBox(
      'VB-CABLE 驱动安装失败（退出码：' + IntToStr(ResultCode) + '）。' + #13#10 +
      '请手动运行：' + SetupExe,
      mbError, MB_OK);
end;

{ ------------------------------------------------------------------ }
{ 挂载点：Task 被勾选时执行                                            }
{ ------------------------------------------------------------------ }
procedure CurStepChanged(CurStep: TSetupStep);
begin
  if (CurStep = ssPostInstall) and IsTaskSelected('install_vbcable') then
    InstallVBCable;
end;
