import { AnimationOptions, AnimationCurve, SpringAnimationOptions, PresetCurveAnimationOptions, CustomCurveAnimationOptions } from 'valdi_core/src/AnimationOptions';
import { CancelToken } from 'valdi_core/src/CancellableAnimation';

/**
 * Manages animations for web elements by tracking active animations
 * and applying CSS transitions/animations to style changes.
 */
export class WebAnimationManager {
  private activeAnimations: Map<CancelToken, AnimationContext> = new Map();
  private animatedElements: Map<number, Set<CancelToken>> = new Map(); // elementId -> set of animation tokens
  private currentAnimationToken: CancelToken | null = null; // Currently active animation during a block
  private nextToken: CancelToken = 1;

  /**
   * Start an animation context
   */
  startAnimation(options: AnimationOptions): CancelToken {
    const token = this.nextToken++;
    const context: AnimationContext = {
      options,
      token,
      animatedProperties: new Set(),
    };
    this.activeAnimations.set(token, context);
    this.currentAnimationToken = token; // Set as current active animation
    return token;
  }

  /**
   * End an animation context
   */
  endAnimation(token: CancelToken): void {
    const context = this.activeAnimations.get(token);
    if (context) {
      // Clean up element tracking
      this.animatedElements.forEach((tokens, elementId) => {
        tokens.delete(token);
        if (tokens.size === 0) {
          this.animatedElements.delete(elementId);
        }
      });
      this.activeAnimations.delete(token);
      
      if (this.currentAnimationToken === token) {
        this.currentAnimationToken = null;
      }
      
      // Call completion callback
      if (context.options.completion) {
        context.options.completion(false);
      }
    }
  }

  /**
   * Cancel an animation
   */
  cancelAnimation(token: CancelToken): void {
    const context = this.activeAnimations.get(token);
    if (context) {
      // Clean up element tracking
      this.animatedElements.forEach((tokens, elementId) => {
        tokens.delete(token);
        if (tokens.size === 0) {
          this.animatedElements.delete(elementId);
        }
      });
      this.activeAnimations.delete(token);
      
      // Call completion callback with cancelled = true
      if (context.options.completion) {
        context.options.completion(true);
      }
    }
  }

  /**
   * Check if an element is currently being animated
   */
  isElementAnimated(elementId: number): boolean {
    return this.animatedElements.has(elementId);
  }

  /**
   * Get animation context for an element (if any)
   * During an animation block, returns the current animation context
   */
  getAnimationContext(elementId: number): AnimationContext | undefined {
    // If we're in an active animation block, return that context
    if (this.currentAnimationToken !== null) {
      const context = this.activeAnimations.get(this.currentAnimationToken);
      if (context) {
        return context;
      }
    }
    
    // Otherwise check if element was previously marked as animated
    const tokens = this.animatedElements.get(elementId);
    if (tokens && tokens.size > 0) {
      // Return the first active animation context
      const token = Array.from(tokens)[0];
      return this.activeAnimations.get(token);
    }
    return undefined;
  }

  /**
   * Mark an element as being animated
   */
  markElementAnimated(elementId: number, token: CancelToken): void {
    if (!this.animatedElements.has(elementId)) {
      this.animatedElements.set(elementId, new Set());
    }
    this.animatedElements.get(elementId)!.add(token);
  }

  /**
   * Convert AnimationOptions to CSS transition string
   */
  getCSSTransition(property: string, options: AnimationOptions): string {
    if ('stiffness' in options) {
      // Spring animation - use a JavaScript-based approach or approximate with CSS
      // For now, approximate spring with a CSS cubic-bezier
      const duration = this.estimateSpringDuration(options as SpringAnimationOptions);
      const timingFunction = this.springToCubicBezier(options as SpringAnimationOptions);
      return `${property} ${duration}s ${timingFunction}`;
    } else {
      // Regular animation
      const duration = options.duration;
      const timingFunction = this.getCSSTimingFunction(options);
      return `${property} ${duration}s ${timingFunction}`;
    }
  }

  /**
   * Get CSS transition for all properties
   */
  getAllPropertiesTransition(options: AnimationOptions): string {
    if ('stiffness' in options) {
      const duration = this.estimateSpringDuration(options as SpringAnimationOptions);
      const timingFunction = this.springToCubicBezier(options as SpringAnimationOptions);
      return `all ${duration}s ${timingFunction}`;
    } else {
      const duration = options.duration;
      const timingFunction = this.getCSSTimingFunction(options);
      return `all ${duration}s ${timingFunction}`;
    }
  }

  /**
   * Convert AnimationCurve to CSS timing function
   */
  private getCSSTimingFunction(options: PresetCurveAnimationOptions | CustomCurveAnimationOptions): string {
    if ('controlPoints' in options && options.controlPoints && options.controlPoints.length === 4) {
      // Custom cubic-bezier
      const [x1, y1, x2, y2] = options.controlPoints;
      return `cubic-bezier(${x1}, ${y1}, ${x2}, ${y2})`;
    } else if ('curve' in options) {
      // Preset curve
      switch (options.curve ?? AnimationCurve.EaseInOut) {
        case AnimationCurve.Linear:
          return 'linear';
        case AnimationCurve.EaseIn:
          return 'ease-in';
        case AnimationCurve.EaseOut:
          return 'ease-out';
        case AnimationCurve.EaseInOut:
          return 'ease-in-out';
      }
    }
    // Default to ease-in-out
    return 'ease-in-out';
  }

  /**
   * Convert spring animation to CSS cubic-bezier approximation
   * This is an approximation - for true spring physics, we'd need JavaScript animation
   */
  private springToCubicBezier(options: SpringAnimationOptions): string {
    // Approximate spring with a bouncy cubic-bezier
    // Higher stiffness = faster, higher damping = less bouncy
    const stiffness = options.stiffness ?? 381.47;
    const damping = options.damping ?? 20.1;
    
    // Normalize to reasonable ranges
    const normalizedStiffness = Math.min(stiffness / 500, 1);
    const normalizedDamping = Math.min(damping / 30, 1);
    
    // Create a bouncy curve based on spring parameters
    // More damping = less bounce (closer to ease-out)
    // More stiffness = faster initial acceleration
    const bounce = 1 - normalizedDamping;
    const x1 = 0.25;
    const y1 = 0.1 + bounce * 0.3;
    const x2 = 0.25 + normalizedStiffness * 0.2;
    const y2 = 1;
    
    return `cubic-bezier(${x1}, ${y1}, ${x2}, ${y2})`;
  }

  /**
   * Estimate duration for spring animation
   * Spring animations don't have a fixed duration, but we need one for CSS
   */
  private estimateSpringDuration(options: SpringAnimationOptions): number {
    const stiffness = options.stiffness ?? 381.47;
    const damping = options.damping ?? 20.1;
    
    // Estimate duration based on spring parameters
    // Higher stiffness = shorter duration
    // Higher damping = shorter duration (less oscillation)
    const baseDuration = 0.5; // Base duration in seconds
    const stiffnessFactor = Math.max(0.3, 1 - (stiffness / 1000));
    const dampingFactor = Math.max(0.5, 1 - (damping / 50));
    
    return baseDuration * stiffnessFactor * dampingFactor;
  }
}

interface AnimationContext {
  options: AnimationOptions;
  token: CancelToken;
  animatedProperties: Set<string>;
}

