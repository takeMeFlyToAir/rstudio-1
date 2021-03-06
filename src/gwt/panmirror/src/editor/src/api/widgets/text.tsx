/*
 * text.tsx
 *
 * Copyright (C) 2019-20 by RStudio, PBC
 *
 * Unless you have received this program directly from RStudio pursuant
 * to the terms of a commercial license agreement with RStudio, then
 * this program is licensed to you under the terms of version 3 of the
 * GNU Affero General Public License. This program is distributed WITHOUT
 * ANY EXPRESS OR IMPLIED WARRANTY, INCLUDING THOSE OF NON-INFRINGEMENT,
 * MERCHANTABILITY OR FITNESS FOR A PARTICULAR PURPOSE. Please refer to the
 * AGPL (http://www.gnu.org/licenses/agpl-3.0.txt) for more details.
 *
 */

import React, { ChangeEventHandler, KeyboardEventHandler, FocusEventHandler } from 'react';

import { WidgetProps } from './react';

import './text.css';

export interface TextInputProps extends WidgetProps {
  width: string;
  tabIndex?: number;
  className?: string;
  placeholder?: string;
  iconAdornment?: string;
  value?: string;
  onChange?: ChangeEventHandler;
  onKeyDown?: KeyboardEventHandler<HTMLInputElement>;
  onKeyUp?: KeyboardEventHandler<HTMLInputElement>;
  onKeyPress?: KeyboardEventHandler<HTMLInputElement>;
  onBlur?: FocusEventHandler<HTMLInputElement>;
}

export const TextInput = React.forwardRef<HTMLInputElement, TextInputProps>((props, ref) => {
  const style: React.CSSProperties = {
    ...props.style,
    width: props.width,
  };

  return (
    <div className="pm-textinput-container" style={style}>
      {
        props.iconAdornment ?
          <img src={props.iconAdornment} className="pm-textinput-icon" alt="" /> :
          undefined
      }
      <input
        type="text"
        placeholder={props.placeholder}
        className={`
          pm-input-text 
          pm-textinput-input 
          pm-text-color 
          pm-background-color 
          ${props.className}
          ${props.iconAdornment ? 'pm-textinput-input-with-icon' : ''}`}
        value={props.value || undefined}
        onChange={props.onChange}
        onKeyDown={props.onKeyDown}
        onKeyUp={props.onKeyUp}
        onKeyPress={props.onKeyPress}
        onBlur={props.onBlur}
        tabIndex={props.tabIndex}
        ref={ref}
      />
    </div>
  );
});
