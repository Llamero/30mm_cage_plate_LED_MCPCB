# -*- mode: python ; coding: utf-8 -*-

block_cipher = None

added_files = [
         ( 'Four Channel MHz LED Driver-download.png', '.' ),
         ( 'Four Channel MHz LED Driver-main.png', '.' ),
         ( 'Four Channel MHz LED Driver-upload.png', '.' ),
         ( 'QtDesigner_GUI.ui', '.' ),
         ( 'Status_GUI.ui', '.' ),
         ( 'Sync_Plot_GUI.ui', '.' ),
         ( 'gui_state.obj', '.' ),
         ]

a = Analysis(['main.py'],
             pathex=['C:\\Users\\Ben\\Documents\\GitHub\\Four_Channel_MHz_LED_Driver\\Software\\GUI_code'],
             binaries=[],
             datas=added_files,
             hiddenimports=[],
             hookspath=[],
             runtime_hooks=[],
             excludes=[],
             win_no_prefer_redirects=False,
             win_private_assemblies=False,
             cipher=block_cipher,
             noarchive=False)
pyz = PYZ(a.pure, a.zipped_data,
             cipher=block_cipher)
exe = EXE(pyz,
          a.scripts,
          a.binaries,
          a.zipfiles,
          a.datas,
          [],
          name='main',
          debug=False,
          bootloader_ignore_signals=False,
          strip=False,
          upx=True,
          upx_exclude=[],
          runtime_tmpdir=None,
          console=True )
