class HIDController {
    constructor() {
        this.touchpad = document.getElementById('touchpad');
        this.leftClick = document.getElementById('leftClick');
        this.rightClick = document.getElementById('rightClick');
        this.connectionStatus = document.getElementById('connectionStatus');
        this.keys = document.querySelectorAll('.key');
        this.lastX = 0;
        this.lastY = 0;
        this.isTouch = 'ontouchstart' in window;
        this.activeModifiers = new Set();
        this.setupEventListeners();
        this.setupScrollButtons();
        this.showKeyboard('lowercase');
    }

    async sendCommand(command) {
        try {
            const response = await fetch('/api/command', {
                method: 'POST',
                headers: {
                    'Content-Type': 'text/plain',
                },
                body: command
            });
            
            if (!response.ok) {
                throw new Error(`HTTP error! status: ${response.status}`);
            }
            
            const text = await response.text();
            if (!text.startsWith('ok:')) {
                throw new Error(text);
            }
        } catch (error) {
            console.error('Error:', error);
            this.connectionStatus.textContent = 'Disconnected';
            this.connectionStatus.classList.add('disconnected');
        }
    }

    showKeyboard(type) {
        this.currentKeyboard = type;
        document.querySelectorAll('.keyboard-section').forEach(section => {
            section.style.display = 'none';
        });
        document.getElementById(`keyboard-${type}`).style.display = 'block';
        
        // Update the keyboard toggle button text
        const toggleBtn = document.querySelector('.keyboard-toggle');
        const toggleText = type === 'lowercase' ? 'ABC' :
                          type === 'uppercase' ? '123' :
                          type === 'numbers' ? '#+=':
                          'abc';
        toggleBtn.textContent = toggleText;
    }

    toggleKeyboard() {
        const keyboards = ['lowercase', 'uppercase', 'numbers', 'symbols'];
        const currentIndex = keyboards.indexOf(this.currentKeyboard);
        const nextIndex = (currentIndex + 1) % keyboards.length;
        this.showKeyboard(keyboards[nextIndex]);
    }

    toggleModifier(modifier, keyCode, lock = false) {
        if (lock) {
            if (this.lockedModifiers.has(keyCode)) {
                this.lockedModifiers.delete(keyCode);
                this.activeModifiers.delete(keyCode);
                modifier.classList.remove('locked');
                modifier.classList.remove('active');
                this.sendCommand(`k:${keyCode}:0`);
            } else {
                this.lockedModifiers.add(keyCode);
                this.activeModifiers.add(keyCode);
                modifier.classList.add('locked');
                modifier.classList.add('active');
                this.sendCommand(`k:${keyCode}:1`);
            }
        } else {
            if (!this.lockedModifiers.has(keyCode)) {
                if (this.activeModifiers.has(keyCode)) {
                    this.activeModifiers.delete(keyCode);
                    modifier.classList.remove('active');
                    this.sendCommand(`k:${keyCode}:0`);
                } else {
                    this.activeModifiers.add(keyCode);
                    modifier.classList.add('active');
                    this.sendCommand(`k:${keyCode}:1`);
                }
            }
        }
    }

    handleModifierToggle(modifier, keyCode) {
        const isActive = modifier.classList.contains('active');
        const isToggle = modifier.dataset.toggle === 'true';
        
        if (isToggle) {
            // For toggle buttons, just toggle the state
            if (isActive) {
                this.activeModifiers.delete(keyCode);
                modifier.classList.remove('active');
                this.sendCommand(`k:${keyCode}:0`);
            } else {
                this.activeModifiers.add(keyCode);
                modifier.classList.add('active');
                this.sendCommand(`k:${keyCode}:1`);
            }
        } else {
            // For non-toggle buttons, activate only while pressed
            if (!isActive) {
                this.activeModifiers.add(keyCode);
                modifier.classList.add('active');
                this.sendCommand(`k:${keyCode}:1`);
            }
        }
    }

    handleModifierRelease(modifier, keyCode) {
        const isToggle = modifier.dataset.toggle === 'true';
        
        // Only release if it's not a toggle button
        if (!isToggle && modifier.classList.contains('active')) {
            this.activeModifiers.delete(keyCode);
            modifier.classList.remove('active');
            this.sendCommand(`k:${keyCode}:0`);
        }
    }

    setupEventListeners() {
        // Touchpad mouse movement
        const moveHandler = (e) => {
            e.preventDefault();
            const rect = this.touchpad.getBoundingClientRect();
            const clientX = this.isTouch ? e.touches[0].clientX : e.clientX;
            const clientY = this.isTouch ? e.touches[0].clientY : e.clientY;
            
            if (this.lastX && this.lastY) {
                const deltaX = Math.round((clientX - this.lastX) * 2);
                const deltaY = Math.round((clientY - this.lastY) * 2);
                if (deltaX !== 0 || deltaY !== 0) {
                    this.sendCommand(`m:${deltaX}:${deltaY}`);
                }
            }
            
            this.lastX = clientX;
            this.lastY = clientY;
        };

        const endHandler = () => {
            this.lastX = 0;
            this.lastY = 0;
        };

        if (this.isTouch) {
            this.touchpad.addEventListener('touchstart', (e) => e.preventDefault());
            this.touchpad.addEventListener('touchmove', moveHandler);
            this.touchpad.addEventListener('touchend', endHandler);
        } else {
            let isMouseDown = false;
            this.touchpad.addEventListener('mousedown', (e) => {
                isMouseDown = true;
                e.preventDefault();
            });
            this.touchpad.addEventListener('mousemove', (e) => {
                if (isMouseDown) moveHandler(e);
            });
            this.touchpad.addEventListener('mouseup', () => {
                isMouseDown = false;
                endHandler();
            });
            this.touchpad.addEventListener('mouseleave', () => {
                if (isMouseDown) endHandler();
                isMouseDown = false;
            });
        }        // Mouse buttons
        const setupButton = (button, buttonNum) => {
            const buttonHandler = (isPress) => {
                button.classList.toggle('active', isPress);
                this.sendCommand(`b:${buttonNum}:${isPress ? 1 : 0}`);
            };

            if (this.isTouch) {
                button.addEventListener('touchstart', (e) => {
                    e.preventDefault();
                    buttonHandler(true);
                });
                button.addEventListener('touchend', (e) => {
                    e.preventDefault();
                    buttonHandler(false);
                });
            } else {
                button.addEventListener('mousedown', () => buttonHandler(true));
                button.addEventListener('mouseup', () => buttonHandler(false));
                button.addEventListener('mouseleave', () => {
                    if (button.classList.contains('active')) {
                        buttonHandler(false);
                    }
                });
            }
        };

        setupButton(this.leftClick, 1);
        setupButton(this.rightClick, 2);

        // Modifier keys
        document.querySelectorAll('.modifier').forEach(modifier => {
            const keyCode = parseInt(modifier.dataset.keycode);
            if (isNaN(keyCode)) return;

            if (this.isTouch) {
                modifier.addEventListener('touchstart', (e) => {
                    e.preventDefault();
                    this.handleModifierToggle(modifier, keyCode);
                });

                modifier.addEventListener('touchend', (e) => {
                    e.preventDefault();
                    this.handleModifierRelease(modifier, keyCode);
                });
            } else {
                modifier.addEventListener('mousedown', () => {
                    this.handleModifierToggle(modifier, keyCode);
                });

                modifier.addEventListener('mouseup', () => {
                    this.handleModifierRelease(modifier, keyCode);
                });

                modifier.addEventListener('mouseleave', () => {
                    this.handleModifierRelease(modifier, keyCode);
                });
            }
        });

        // Regular keys
        document.querySelectorAll('.key:not(.modifier):not(.keyboard-toggle)').forEach(key => {
            const keyCode = parseInt(key.dataset.keycode);
            const hasShift = key.classList.contains('shift');
            
            if (isNaN(keyCode)) return;

            const keyHandler = async (isPress) => {
                key.classList.toggle('active', isPress);
                
                try {
                    if (isPress) {
                        // Send active modifiers first
                        for (const modCode of this.activeModifiers) {
                            await this.sendCommand(`k:${modCode}:1`);
                        }

                        // For ASCII symbols, just send the keycode
                        await this.sendCommand(`k:${keyCode}:1`);
                    } else {
                        // Release the key
                        await this.sendCommand(`k:${keyCode}:0`);

                        // Release non-toggle modifiers
                        document.querySelectorAll('.modifier:not([data-toggle="true"])').forEach(async (mod) => {
                            if (mod.classList.contains('active')) {
                                const modCode = parseInt(mod.dataset.keycode);
                                if (!isNaN(modCode)) {
                                    await this.sendCommand(`k:${modCode}:0`);
                                    mod.classList.remove('active');
                                    this.activeModifiers.delete(modCode);
                                }
                            }
                        });
                    }
                } catch (error) {
                    console.error('Error sending key command:', error);
                }
            };

            if (this.isTouch) {
                key.addEventListener('touchstart', (e) => {
                    e.preventDefault();
                    keyHandler(true);
                });
                key.addEventListener('touchend', (e) => {
                    e.preventDefault();
                    keyHandler(false);
                });
            } else {
                key.addEventListener('mousedown', () => keyHandler(true));
                key.addEventListener('mouseup', () => keyHandler(false));
                key.addEventListener('mouseleave', () => {
                    if (key.classList.contains('active')) {
                        keyHandler(false);
                    }
                });
            }
        });

        // Keyboard toggle button
        document.querySelectorAll('.keyboard-toggle').forEach(toggle => {
            if (this.isTouch) {
                toggle.addEventListener('touchend', (e) => {
                    e.preventDefault();
                    this.toggleKeyboard();
                });
            } else {
                toggle.addEventListener('click', () => this.toggleKeyboard());
            }
        });
    }

    setupScrollButtons() {
        const scrollHandler = (button) => {
            const keyCode = parseInt(button.dataset.keycode);
            if (isNaN(keyCode)) return;

            let isScrolling = false;
            let scrollInterval;

            const startScroll = () => {
                if (!isScrolling) {
                    isScrolling = true;
                    scrollInterval = setInterval(() => {
                        this.sendCommand(`k:${keyCode}:1`);
                    }, 100); // Repeat every 100ms while held
                }
            };

            const stopScroll = () => {
                if (isScrolling) {
                    isScrolling = false;
                    clearInterval(scrollInterval);
                }
            };

            if (this.isTouch) {
                button.addEventListener('touchstart', (e) => {
                    e.preventDefault();
                    startScroll();
                });
                button.addEventListener('touchend', (e) => {
                    e.preventDefault();
                    stopScroll();
                });
            } else {
                button.addEventListener('mousedown', startScroll);
                button.addEventListener('mouseup', stopScroll);
                button.addEventListener('mouseleave', stopScroll);
            }
        };

        // Set up scroll buttons
        document.querySelectorAll('#scrollUp, #scrollDown, #scrollLeft, #scrollRight').forEach(button => {
            scrollHandler(button);
        });
    }
}

// Initialize controller when page loads
document.addEventListener('DOMContentLoaded', () => {
    new HIDController();
});
