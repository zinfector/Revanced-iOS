"""Windows-friendly interface for the version-specific YouTube IPA patcher."""
import json
from pathlib import Path
import queue
import sys
import threading
import tkinter as tk
from tkinter import filedialog, messagebox, ttk

import patcher
from features import CATALOG as LABELS




class App:
    def __init__(self, root):
        self.root = root
        root.title('YouTube iOS Patcher 0.3.47 (merged) - 21.39.4')
        root.geometry('740x720')
        root.minsize(660, 680)
        self.events = queue.Queue()
        self.busy = False
        self.config = {}
        self.branding = {}
        self.input = tk.StringVar()
        self.output = tk.StringVar()
        self.speed = tk.StringVar(value='1.0')
        self.quality = tk.StringVar(value='0')
        self.strategy = tk.StringVar(value='response')
        self.miniplayer_size = tk.StringVar(value='0')
        self.miniplayer_opacity = tk.StringVar(value='1.0')
        self.strip = tk.BooleanVar(value=True)
        self.flags = {key: tk.BooleanVar(value=patcher.DEFAULTS[key]) for key in patcher.FEATURES}
        frame = ttk.Frame(root, padding=18)
        frame.pack(fill='both', expand=True)
        frame.columnconfigure(1, weight=1)
        ttk.Label(frame, text='YouTube iOS Patcher', font=('Segoe UI', 18, 'bold')).grid(row=0, column=0, columnspan=3, sticky='w')
        ttk.Label(frame, text='Supports the analyzed, decrypted YouTube 21.39.4 ARM64 build.').grid(row=1, column=0, columnspan=3, sticky='w', pady=(3, 14))
        for row, text, variable, callback in ((2, 'Original IPA', self.input, self.choose_input), (3, 'Output IPA', self.output, self.choose_output)):
            ttk.Label(frame, text=text).grid(row=row, column=0, sticky='w', padx=(0, 12))
            ttk.Entry(frame, textvariable=variable).grid(row=row, column=1, sticky='ew', pady=4)
            ttk.Button(frame, text='Browse…', command=callback).grid(row=row, column=2, padx=(8, 0))
        options = ttk.LabelFrame(frame, text='Feature candidates — all require device testing', padding=8)
        options.grid(row=4, column=0, columnspan=3, sticky='nsew', pady=12)
        canvas = tk.Canvas(options, height=190, highlightthickness=0)
        scrollbar = ttk.Scrollbar(options, orient='vertical', command=canvas.yview)
        canvas.configure(yscrollcommand=scrollbar.set)
        scrollbar.pack(side='right', fill='y');canvas.pack(side='left', fill='both', expand=True)
        checks = ttk.Frame(canvas)
        canvas.create_window((0,0), window=checks, anchor='nw')
        checks.bind('<Configure>',lambda e: canvas.configure(scrollregion=canvas.bbox('all')))
        canvas.bind('<MouseWheel>',lambda e: canvas.yview_scroll(-int(e.delta/120),'units'))
        for key in patcher.FEATURES:
            ttk.Checkbutton(checks, text=LABELS[key], variable=self.flags[key]).pack(anchor='w', pady=2)
        playback = ttk.Frame(frame)
        playback.grid(row=5, column=0, columnspan=3, sticky='ew')
        for column, (label, variable, values) in enumerate((
            ('Speed', self.speed, ('1.0', '0.5', '0.75', '1.25', '1.5', '1.75', '2.0', '2.5', '3.0', '4.0')),
            ('Resolution cap (0 = Auto)', self.quality, ('0', '144', '240', '360', '480', '720', '1080', '1440', '2160')),
            ('Ad strategy', self.strategy, ('response', 'trigger', 'coordinator')),
        )):
            box = ttk.Frame(playback)
            box.grid(row=0, column=column, sticky='w', padx=(0, 16))
            ttk.Label(box, text=label).pack(anchor='w')
            ttk.Combobox(box, textvariable=variable, values=values, width=20, state='readonly').pack(anchor='w', pady=4)
        for column, (label, variable, values) in enumerate((
            ('Miniplayer size (0 = native)', self.miniplayer_size, ('0', '170', '192', '240', '300', '360', '480')),
            ('Miniplayer background opacity', self.miniplayer_opacity, ('0.0', '0.25', '0.5', '0.75', '1.0')),
        )):
            box = ttk.Frame(playback)
            box.grid(row=1, column=column, sticky='w', padx=(0, 16))
            ttk.Label(box, text=label).pack(anchor='w')
            ttk.Combobox(box, textvariable=variable, values=values, width=24, state='readonly').pack(anchor='w', pady=4)
        ttk.Checkbutton(frame, text='Remove app extensions (recommended for SideStore and other sideloaders)', variable=self.strip).grid(row=6, column=0, columnspan=3, sticky='w', pady=(10, 6))
        ttk.Label(frame, text='Creates an unsigned IPA. Re-sign before installing. Device behavior is untested.\nIn the app, hold three fingers for one second to open patch settings.', wraplength=680).grid(row=7, column=0, columnspan=3, sticky='w', pady=8)
        actions = ttk.Frame(frame)
        actions.grid(row=8, column=0, columnspan=3, sticky='ew', pady=8)
        self.buttons = [ttk.Button(actions, text=text, command=command) for text, command in (
            ('Inspect original', self.do_inspect), ('Create patched IPA', self.do_patch), ('Verify output', self.do_verify), ('Load config', self.load_config), ('Branding', self.edit_branding))]
        for number,button in enumerate(self.buttons):
            button.grid(row=number//3,column=number%3,sticky='w',padx=(0,8),pady=3)
        self.progress = ttk.Progressbar(frame, mode='indeterminate')
        self.progress.grid(row=9, column=0, columnspan=3, sticky='ew', pady=(0, 8))
        self.log = tk.Text(frame, height=7, wrap='word', state='disabled', font=('Consolas', 10))
        self.log.grid(row=10, column=0, columnspan=3, sticky='nsew')
        frame.rowconfigure(10, weight=1)
        root.protocol('WM_DELETE_WINDOW', self.close)
        root.after(100, self.poll)
        self.write('Ready. The original IPA will be preserved. Existing output files are never overwritten.')

    def write(self, message):
        self.log.configure(state='normal')
        self.log.insert('end', message + '\n')
        self.log.see('end')
        self.log.configure(state='disabled')

    def choose_input(self):
        if self.busy: return
        value = filedialog.askopenfilename(filetypes=[('iOS app archive', '*.ipa')])
        if value:
            self.input.set(value)
            path = Path(value)
            self.output.set(str(path.with_name(path.stem + '-RVPort-unsigned.ipa')))

    def choose_output(self):
        if self.busy: return
        value = filedialog.asksaveasfilename(defaultextension='.ipa', filetypes=[('iOS app archive', '*.ipa')])
        if value: self.output.set(value)

    def start(self, label, work):
        if self.busy: return
        self.busy = True
        for button in self.buttons: button.configure(state='disabled')
        self.progress.start()
        self.write(label)
        def run():
            try: self.events.put(('ok', work()))
            except Exception as ex: self.events.put(('error', str(ex)))
        threading.Thread(target=run, daemon=False).start()

    def poll(self):
        try:
            kind, result = self.events.get_nowait()
        except queue.Empty:
            pass
        else:
            self.busy = False
            self.progress.stop()
            for button in self.buttons: button.configure(state='normal')
            self.write(json.dumps(result, indent=2) if kind == 'ok' else 'Error: ' + result)
            if kind == 'error': messagebox.showerror('Patcher', result)
        self.root.after(100, self.poll)

    def do_inspect(self):
        path = self.input.get().strip()
        if not path: return messagebox.showerror('Patcher', 'Choose an original IPA.')
        self.start('Inspecting input…', lambda: patcher.inspect(path))

    def do_verify(self):
        path = self.output.get().strip()
        if not path: return messagebox.showerror('Patcher', 'Choose an output IPA.')
        self.start('Verifying unsigned output…', lambda: patcher.verify(path))

    def do_patch(self):
        source, target = self.input.get().strip(), self.output.get().strip()
        if not source or not target: return messagebox.showerror('Patcher', 'Choose both input and output paths.')
        config = dict(self.config, **{key: flag.get() for key, flag in self.flags.items()})
        config.update(default_speed=float(self.speed.get()), default_quality=int(self.quality.get()), ad_strategy=self.strategy.get(), miniplayer_min_dimension_points=float(self.miniplayer_size.get()), miniplayer_overlay_opacity=float(self.miniplayer_opacity.get()))
        strip = self.strip.get()
        branding = dict(self.branding)
        def work():
            manifest = patcher.patch(source, target, patcher.ROOT/'build/RVPort.dylib', config, strip,branding)
            return {'output': target, 'signing_required': True, 'device_validated': False, 'extensions_removed': manifest['extensions_removed'], 'features': {key: manifest['config'][key] for key in patcher.FEATURES}}
        self.start('Creating and verifying the patched IPA. Please keep this window open…', work)

    def load_config(self):
        if self.busy: return
        path=filedialog.askopenfilename(filetypes=[('Patch configuration','*.json')])
        if not path:return
        try:
            self.config=patcher.validate_config(json.loads(Path(path).read_text(encoding='utf-8')))
            for key, flag in self.flags.items():flag.set(self.config[key])
            self.speed.set(str(self.config['default_speed']));self.quality.set(str(self.config['default_quality']));self.strategy.set(self.config['ad_strategy'])
            self.miniplayer_size.set(str(self.config['miniplayer_min_dimension_points']));self.miniplayer_opacity.set(str(self.config['miniplayer_overlay_opacity']))
            self.write('Loaded configuration: '+path)
        except Exception as ex:messagebox.showerror('Patcher',str(ex))

    def edit_branding(self):
        if self.busy:return
        dialog=tk.Toplevel(self.root);dialog.title('Optional branding');dialog.transient(self.root);dialog.grab_set()
        frame=ttk.Frame(dialog,padding=16);frame.pack(fill='both',expand=True)
        name=tk.StringVar(value=self.config.get('app_name',''))
        ttk.Label(frame,text='Display name (empty keeps original)').grid(row=0,column=0,sticky='w')
        ttk.Entry(frame,textvariable=name,width=46).grid(row=0,column=1,columnspan=2,sticky='ew',pady=5)
        variables={}
        for row,(key,label) in enumerate((('header_image','Header PNG'),('icon_120','Phone icon 120x120 PNG'),('icon_180','Phone icon 180x180 PNG'),('icon_152','iPad icon 152x152 PNG')),start=1):
            variable=tk.StringVar(value=str(self.branding.get(key,'')));variables[key]=variable
            ttk.Label(frame,text=label).grid(row=row,column=0,sticky='w',padx=(0,10))
            ttk.Entry(frame,textvariable=variable,width=46).grid(row=row,column=1,sticky='ew',pady=5)
            def choose(target=variable):
                path=filedialog.askopenfilename(parent=dialog,filetypes=[('PNG image','*.png')])
                if path:target.set(path)
            ttk.Button(frame,text='Browse',command=choose).grid(row=row,column=2,padx=(6,0))
        ttk.Label(frame,text='Phone icons require both sizes. PNGs are validated without resizing.\nClear a field to keep that original resource.').grid(row=5,column=0,columnspan=3,sticky='w',pady=10)
        def save():
            assets={key:Path(variable.get()) for key,variable in variables.items() if variable.get().strip()}
            try:
                patcher.validate_config(dict(self.config,app_name=name.get()))
                from branding import prepare
                prepare({},assets)
            except Exception as ex:return messagebox.showerror('Branding',str(ex),parent=dialog)
            self.config['app_name']=name.get();self.branding=assets;dialog.destroy();self.write('Branding options saved.')
        ttk.Button(frame,text='Save',command=save).grid(row=6,column=1,sticky='e',pady=6)
        ttk.Button(frame,text='Cancel',command=dialog.destroy).grid(row=6,column=2,padx=(6,0))

    def close(self):
        if self.busy:
            messagebox.showinfo('Patcher', 'The current operation is still running. Wait for completion before closing.')
        else: self.root.destroy()


def main():
    # Runs without displaying a window, including in the packaged executable.
    if len(sys.argv) == 3 and sys.argv[1] == '--self-test':
        try:
            profile = patcher.profile()
            library = patcher.payload(patcher.ROOT/'build/RVPort.dylib')
            root = tk.Tk()
            root.withdraw()
            app = App(root)
            if not app.strip.get():
                raise RuntimeError("GUI must remove extensions by default for sideloading")
            root.update()
            root.destroy()
            Path(sys.argv[2]).write_text(json.dumps({'status': 'ok', 'profile': profile['id'], 'payload_sha256': patcher.sha(library), 'feature_switches':len(patcher.FEATURES), 'tk_interface_initialized': True, 'strip_extensions_default': True}), encoding='utf-8')
        except Exception as ex:
            Path(sys.argv[2]).write_text(json.dumps({'status': 'error', 'error': str(ex)}), encoding='utf-8')
            return 2
        return 0
    root = tk.Tk()
    App(root)
    root.mainloop()
    return 0


if __name__ == '__main__':
    raise SystemExit(main())
