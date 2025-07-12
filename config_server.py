#!/usr/bin/env python3
"""
Lightweight web server to modify configuration files and restart the display process.
"""
import os
import sys
import json
import subprocess
from http.server import HTTPServer, BaseHTTPRequestHandler
import urllib.parse

# Load UI configuration
def load_ui_config(ui_config_path):
    config = {}
    try:
        with open(ui_config_path, 'r') as f:
            for line in f:
                line = line.strip()
                if line and not line.startswith('#'):
                    key, value = line.split(' - ', 1)
                    config[key] = value
        return config
    except Exception as e:
        print(f"Error loading UI config: {e}")
        sys.exit(1)

# Load config file
def load_config(config_path):
    config = {}
    ordered_keys = []  # To maintain the original order
    try:
        with open(config_path, 'r') as f:
            for line in f:
                line = line.strip()
                if line and not line.startswith('#'):
                    parts = line.split('=', 1)
                    if len(parts) == 2:
                        key, value = parts
                        key = key.strip()
                        config[key] = value.strip()
                        ordered_keys.append(key)
        return config, ordered_keys
    except Exception as e:
        print(f"Error loading config: {e}")
        return {}, []

# Save config file
def save_config(config_path, config):
    try:
        # Read the original file to preserve comments and formatting
        with open(config_path, 'r') as f:
            lines = f.readlines()
        
        # Update the values
        for i, line in enumerate(lines):
            if line.strip() and not line.strip().startswith('#'):
                parts = line.split('=', 1)
                if len(parts) == 2:
                    key = parts[0].strip()
                    if key in config:
                        lines[i] = f"{key}={config[key]}\n"
        
        # Write back to the file
        with open(config_path, 'w') as f:
            f.writelines(lines)
        return True
    except Exception as e:
        print(f"Error saving config: {e}")
        return False

# Restart the process
def restart_process(ui_config):
    try:
        exec_dir = ui_config.get("Executable_directory")
        cmd_line = ui_config.get("Executable_command_line")
        
        # We no longer need to replace placeholders since the command line is explicit
        # Just execute the command directly
        
        # Kill existing process if it's running
        subprocess.run(["pkill", "-f", "departureboard"], stderr=subprocess.PIPE)
        
        # Start the new process
        subprocess.Popen(cmd_line, shell=True)
        return True
    except Exception as e:
        print(f"Error restarting process: {e}")
        return False

class ConfigHandler(BaseHTTPRequestHandler):
    def __init__(self, *args, ui_config=None, **kwargs):
        self.ui_config = ui_config
        self.config_path = os.path.join(
            ui_config.get("Executable_directory", ""), 
            "config.txt"
        )
        super().__init__(*args, **kwargs)
    
    def do_GET(self):
        if self.path == '/':
            self.send_response(200)
            self.send_header('Content-type', 'text/html')
            self.end_headers()
            
            config, ordered_keys = load_config(self.config_path)
            
            html = f'''
            <!DOCTYPE html>
            <html>
            <head>
                <title>Train Display Configuration</title>
                <meta name="viewport" content="width=device-width, initial-scale=1">
                <style>
                    body {{
                        font-family: Arial, sans-serif;
                        max-width: 800px;
                        margin: 0 auto;
                        padding: 20px;
                    }}
                    .form-group {{
                        margin-bottom: 15px;
                    }}
                    label {{
                        display: inline-block;
                        width: 200px;
                        font-weight: bold;
                    }}
                    input {{
                        width: 300px;
                        padding: 8px;
                        box-sizing: border-box;
                    }}
                    button {{
                        background-color: #4CAF50;
                        color: white;
                        padding: 10px 15px;
                        border: none;
                        cursor: pointer;
                        font-size: 16px;
                    }}
                    .success {{
                        color: green;
                        font-weight: bold;
                    }}
                    .error {{
                        color: red;
                        font-weight: bold;
                    }}
                </style>
            </head>
            <body>
                <h1>Train Display Configuration</h1>
                <div class="button-group" style="margin-bottom: 20px;">
                    <button type="button" id="resetButton" style="background-color: #f44336; margin-right: 10px;">Reset to Defaults</button>
                    <button type="button" id="saveDefaultButton" style="background-color: #2196F3;">Save as Default</button>
                </div>
                <form id="configForm">
            '''
            
            # Add fields for each config parameter in the original order
            for key in ordered_keys:
                value = config[key]
                html += f'''
                <div class="form-group">
                    <label for="{key}">{key}:</label>
                    <input type="text" id="{key}" name="{key}" value="{value}">
                </div>
                '''
            
            html += '''
                    <button type="submit">Save and Restart</button>
                    <div id="message"></div>
                </form>
                
                <script>
                    // Function to update form with given config values
                    function updateFormWithConfig(config) {
                        for (const key in config) {
                            const inputField = document.getElementById(key);
                            if (inputField) {
                                inputField.value = config[key];
                            }
                        }
                    }
                    
                    // Reset button click handler
                    document.getElementById('resetButton').addEventListener('click', function() {
                        fetch('/reset')
                            .then(response => response.json())
                            .then(defaultConfig => {
                                if (defaultConfig.error) {
                                    const messageDiv = document.getElementById('message');
                                    messageDiv.className = 'error';
                                    messageDiv.textContent = 'Error: ' + defaultConfig.error;
                                } else {
                                    updateFormWithConfig(defaultConfig);
                                    const messageDiv = document.getElementById('message');
                                    messageDiv.className = 'success';
                                    messageDiv.textContent = 'Form reset to default values. Click Save and Restart to apply changes.';
                                }
                            })
                            .catch(error => {
                                const messageDiv = document.getElementById('message');
                                messageDiv.className = 'error';
                                messageDiv.textContent = 'Error: ' + error;
                            });
                    });
                    
                    // Save as Default button click handler
                    document.getElementById('saveDefaultButton').addEventListener('click', function() {
                        // Collect current form values
                        const formData = new FormData(document.getElementById('configForm'));
                        const data = {};
                        for (const [key, value] of formData.entries()) {
                            data[key] = value;
                        }
                        
                        // Send to server to save as default
                        fetch('/savedefault', {
                            method: 'POST',
                            headers: {
                                'Content-Type': 'application/json'
                            },
                            body: JSON.stringify(data)
                        })
                        .then(response => response.json())
                        .then(result => {
                            const messageDiv = document.getElementById('message');
                            if (result.success) {
                                messageDiv.className = 'success';
                                messageDiv.textContent = 'Current settings saved as default configuration.';
                            } else {
                                messageDiv.className = 'error';
                                messageDiv.textContent = 'Error: ' + result.error;
                            }
                        })
                        .catch(error => {
                            const messageDiv = document.getElementById('message');
                            messageDiv.className = 'error';
                            messageDiv.textContent = 'Error: ' + error;
                        });
                    });
                    
                    // Form submission handler
                    document.getElementById('configForm').addEventListener('submit', function(e) {
                        e.preventDefault();
                        
                        const formData = new FormData(this);
                        const data = {};
                        for (const [key, value] of formData.entries()) {
                            data[key] = value;
                        }
                        
                        fetch('/save', {
                            method: 'POST',
                            headers: {
                                'Content-Type': 'application/json'
                            },
                            body: JSON.stringify(data)
                        })
                        .then(response => response.json())
                        .then(result => {
                            const messageDiv = document.getElementById('message');
                            if (result.success) {
                                messageDiv.className = 'success';
                                messageDiv.textContent = 'Configuration saved and process restarted successfully!';
                            } else {
                                messageDiv.className = 'error';
                                messageDiv.textContent = 'Error: ' + result.error;
                            }
                        })
                        .catch(error => {
                            const messageDiv = document.getElementById('message');
                            messageDiv.className = 'error';
                            messageDiv.textContent = 'Error: ' + error;
                        });
                    });
                </script>
            </body>
            </html>
            '''
            
            self.wfile.write(html.encode())
        elif self.path == '/reset':
            self.send_response(200)
            self.send_header('Content-type', 'application/json')
            self.end_headers()
            
            default_config_path = self.ui_config.get("Defaults_Config_file")
            if os.path.exists(default_config_path):
                default_config, _ = load_config(default_config_path)
                self.wfile.write(json.dumps(default_config).encode())
            else:
                self.wfile.write(json.dumps({"error": "Default config file not found"}).encode())
        else:
            self.send_response(404)
            self.end_headers()
    
    def do_POST(self):
        if self.path == '/save':
            content_length = int(self.headers['Content-Length'])
            post_data = self.rfile.read(content_length).decode('utf-8')
            data = json.loads(post_data)
            
            # Save the config
            success = save_config(self.config_path, data)
            
            # Restart the process if save was successful
            if success:
                restart_success = restart_process(self.ui_config)
                if not restart_success:
                    success = False
                    error = "Failed to restart the process"
            else:
                error = "Failed to save configuration"
            
            # Send response
            self.send_response(200)
            self.send_header('Content-type', 'application/json')
            self.end_headers()
            
            response = {
                'success': success
            }
            if not success:
                response['error'] = error
                
            self.wfile.write(json.dumps(response).encode())
        elif self.path == '/savedefault':
            content_length = int(self.headers['Content-Length'])
            post_data = self.rfile.read(content_length).decode('utf-8')
            data = json.loads(post_data)
            
            # Get the path to the default config file
            default_config_path = self.ui_config.get("Defaults_Config_file")
            
            # Save the current settings as default
            success = save_config(default_config_path, data)
            
            # Send response
            self.send_response(200)
            self.send_header('Content-type', 'application/json')
            self.end_headers()
            
            response = {
                'success': success
            }
            if not success:
                response['error'] = "Failed to save default configuration"
                
            self.wfile.write(json.dumps(response).encode())
        else:
            self.send_response(404)
            self.end_headers()

def run(server_class=HTTPServer, handler_class=ConfigHandler, ui_config=None):
    port = int(ui_config.get("Port", 8080))
    server_address = ('', port)
    
    # Create a custom handler with the UI config
    def handler(*args, **kwargs):
        return handler_class(*args, ui_config=ui_config, **kwargs)
    
    httpd = server_class(server_address, handler)
    print(f"Starting server on port {port}...")
    try:
        httpd.serve_forever()
    except KeyboardInterrupt:
        pass
    httpd.server_close()
    print("Server stopped.")

if __name__ == "__main__":
    script_dir = os.path.dirname(os.path.abspath(__file__))
    ui_config_path = os.path.join(script_dir, "ui-config.txt")
    
    if not os.path.exists(ui_config_path):
        print(f"UI config file not found: {ui_config_path}")
        sys.exit(1)
    
    ui_config = load_ui_config(ui_config_path)
    run(ui_config=ui_config)
